/* TimeCoordinator Implementation
 * 
 * Single Source of Truth for all time-related operations.
 */

#include "time_coordinator.h"
#include "config.h"
#include "wifi_manager.h"  // For ramTimezoneMode
#include "_locale.h"       // For TXT_ strings

// ═══════════════════════════════════════════════════════════════════════════
// Static Members
// ═══════════════════════════════════════════════════════════════════════════

RTC_DATA_ATTR bool TimeCoordinator::rtcSynced_ = false;

// ═══════════════════════════════════════════════════════════════════════════
// Public Methods
// ═══════════════════════════════════════════════════════════════════════════

void TimeCoordinator::begin() {
    mode_ = (ramTimezoneMode == TIMEZONE_MODE_AUTO) ? 
            TIME_MODE_AUTO : TIME_MODE_MANUAL;
    
    // Parse manual timezone offset if needed
    if (mode_ == TIME_MODE_MANUAL) {
        manualOffsetSeconds_ = parseTimezoneString_(TIMEZONE);
    } else {
        manualOffsetSeconds_ = 0;
    }
    
    Serial.printf("[TimeCoordinator] Mode: %s, ManualOffset: %+ds\n", 
                  mode_ == TIME_MODE_AUTO ? "AUTO" : "MANUAL",
                  manualOffsetSeconds_);
}

TimeDisplayData TimeCoordinator::process(const owm_resp_onecall_t& apiData,
                                         unsigned long startTimeMillis) {
    TimeDisplayData out = {};  // Zero-initialize
    
    // ═══════════════════════════════════════════════════════════════════════
    // ETAPA 1: Normalização (cópia, NÃO muta apiData)
    // ═══════════════════════════════════════════════════════════════════════
    NormalizedWeather norm;
    normalize_(apiData, norm);
    
    // ═══════════════════════════════════════════════════════════════════════
    // ETAPA 2: Sync RTC se necessário (ANTES de usar time())
    // ═══════════════════════════════════════════════════════════════════════
    if (mode_ == TIME_MODE_AUTO) {
        syncRtcIfNeeded_(norm.apiOffsetSeconds);
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // ETAPA 3: Pega hora atual
    // ═══════════════════════════════════════════════════════════════════════
    time_t now = time(nullptr);
    
    if (mode_ == TIME_MODE_MANUAL) {
        // MANUAL: RTC está em UTC0, converte para local
        now = utcToLocal_(now);
    }
    // AUTO: RTC já está em hora local (sincronizado acima)
    
    // ═══════════════════════════════════════════════════════════════════════
    // ETAPA 4: Formata tudo para UI
    // ═══════════════════════════════════════════════════════════════════════
    formatDisplayData_(norm, out, now, startTimeMillis);
    
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// Private Methods
// ═══════════════════════════════════════════════════════════════════════════

void TimeCoordinator::normalize_(const owm_resp_onecall_t& src, 
                                 NormalizedWeather& dst) {
    dst.apiOffsetSeconds = src.timezone_offset;
    
    // Current weather
    if (mode_ == TIME_MODE_MANUAL) {
        dst.currentDt = utcToLocal_(src.current.dt);
    } else {
        dst.currentDt = src.current.dt;  // Already local in AUTO
    }
    
    // Daily forecast (coalesced struct for cache locality)
    for (int i = 0; i < OWM_NUM_DAILY; i++) {
        if (mode_ == TIME_MODE_MANUAL) {
            dst.daily[i].dt = utcToLocal_(src.daily[i].dt);
            dst.daily[i].sunrise = utcToLocal_(src.daily[i].sunrise);
            dst.daily[i].sunset = utcToLocal_(src.daily[i].sunset);
        } else {
            dst.daily[i].dt = src.daily[i].dt;
            dst.daily[i].sunrise = src.daily[i].sunrise;
            dst.daily[i].sunset = src.daily[i].sunset;
        }
    }
    
    // Hourly forecast
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        if (mode_ == TIME_MODE_MANUAL) {
            dst.hourlyDt[i] = utcToLocal_(src.hourly[i].dt);
        } else {
            dst.hourlyDt[i] = src.hourly[i].dt;
        }
    }
}

void TimeCoordinator::syncRtcIfNeeded_(int offsetSeconds) {
    // Check 1: Sessão atual já sincronizou?
    if (rtcSynced_) {
        Serial.println("[TimeCoordinator] RTC already synced this session");
        return;
    }
    
    // Check 2: RTC sobreviveu ao deep sleep? (epoch razoável)
    time_t currentTime = time(nullptr);
    if (currentTime > 1000000000) {  // > 2001-09-09
        Serial.println("[TimeCoordinator] RTC time valid, skip sync");
        rtcSynced_ = true;
        return;
    }
    
    // Precisa sincronizar
    Serial.printf("[TimeCoordinator] Syncing RTC to UTC%+d\n", offsetSeconds);
    
    // Configura timezone e NTP
    configTime(offsetSeconds, 0, "pool.ntp.org");
    
    // Pequeno delay para estabilizar
    delay(100);
    
    // Verifica se conseguiu
    time_t after = time(nullptr);
    if (after > 1000000000) {
        Serial.printf("[TimeCoordinator] RTC synced, time: %ld\n", (long)after);
        rtcSynced_ = true;
    } else {
        Serial.println("[TimeCoordinator] WARNING: RTC sync may have failed");
    }
}

void TimeCoordinator::formatDisplayData_(const NormalizedWeather& norm,
                                        TimeDisplayData& out,
                                        time_t nowLocal,
                                        unsigned long startTimeMillis) {
    tm* tmNow = localtime(&nowLocal);
    
    // Hora para footer (ex: "14:32")
    snprintf(out.updateTime, sizeof(out.updateTime), 
             "%02d:%02d", tmNow->tm_hour, tmNow->tm_min);
    
    // Data para header (ex: "Ter, 15 Abr 2025")
    // Usa formato localizado se disponível
    strftime(out.displayDate, sizeof(out.displayDate), 
             "%a, %d %b %Y", tmNow);
    
    // Forecast - dia da semana para cada dia
    for (int i = 0; i < OWM_NUM_DAILY; i++) {
        tm* tmDaily = localtime(&norm.daily[i].dt);
        out.forecastDayOfWeek[i] = tmDaily->tm_wday;
    }
    
    // Hourly labels para outlook graph (ex: "14h", "15h")
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        tm* tmHour = localtime(&norm.hourlyDt[i]);
        snprintf(out.hourlyLabels[i], sizeof(out.hourlyLabels[0]), 
                 "%02dh", tmHour->tm_hour);
    }
    
    // Sleep duration
    out.sleepDurationSeconds = calculateSleep_(nowLocal);
}

uint64_t TimeCoordinator::calculateSleep_(time_t nowLocal) {
    tm* tmNow = localtime(&nowLocal);
    
    int curHour = tmNow->tm_hour;
    int curMin = tmNow->tm_min;
    int curSec = tmNow->tm_sec;
    
    // Calcula tempo relativo a WAKE_TIME
    int bedtimeHour = INT_MAX;
    if (BED_TIME != WAKE_TIME) {
        bedtimeHour = (BED_TIME - WAKE_TIME + 24) % 24;
    }
    
    int relHour = (curHour - WAKE_TIME + 24) % 24;
    int curMinute = relHour * 60 + curMin;
    int curSecond = relHour * 3600 + curMin * 60 + curSec;
    
    // Calcula minutos até próximo update
    int sleepMinutes = SLEEP_DURATION - (curMinute % SLEEP_DURATION);
    if (sleepMinutes < SLEEP_DURATION / 2) {
        sleepMinutes += SLEEP_DURATION;
    }
    
    // Verifica se cai em período de sono
    int predictedWakeHour = ((curMinute + sleepMinutes) / 60) % 24;
    
    uint64_t sleepDuration;
    if (predictedWakeHour < bedtimeHour) {
        sleepDuration = sleepMinutes * 60 - curSec;
    } else {
        int hoursUntilWake = 24 - relHour;
        sleepDuration = hoursUntilWake * 3600ULL - (curMin * 60ULL + curSec);
    }
    
    // Compensação para RTCs rápidos
    sleepDuration += 3ULL;
    sleepDuration *= 1.0015f;
    
    return sleepDuration;
}

// ═══════════════════════════════════════════════════════════════════════════
// Timezone Parsing
// ═══════════════════════════════════════════════════════════════════════════

int TimeCoordinator::parseTimezoneString_(const char* tzString) {
    // Parse simplificado de string TZ POSIX
    // Exemplos: "EST5EDT,M3.2.0,M11.1.0" ou "CET-1CEST,M3.5.0,M10.5.0/3"
    
    if (tzString == nullptr || strlen(tzString) == 0) {
        return 0;  // UTC
    }
    
    // Procura por número na string (offset)
    // Formato típico: "EST5" = UTC-5, "CET-1" = UTC+1
    int offset = 0;
    bool negative = false;
    const char* p = tzString;
    
    // Pula nome do timezone
    while (*p && ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) {
        p++;
    }
    
    // Verifica sinal
    if (*p == '-') {
        negative = true;
        p++;
    } else if (*p == '+') {
        p++;
    }
    
    // Parse número (horas)
    while (*p >= '0' && *p <= '9') {
        offset = offset * 10 + (*p - '0');
        p++;
    }
    
    // Converte para segundos
    int offsetSeconds = offset * 3600;
    
    // Ajusta sinal (POSIX TZ: positive = west, negative = east)
    // Queremos: positive = east (futuro), negative = west (passado)
    if (!negative) {
        offsetSeconds = -offsetSeconds;
    }
    
    Serial.printf("[TimeCoordinator] Parsed TZ '%s' as UTC%+ds\n", 
                  tzString, offsetSeconds);
    
    return offsetSeconds;
}
