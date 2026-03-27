# Plano: Adicionar Informação de Vento ao Widget de Umbrella

## 🎯 Objetivo
Adicionar informação de velocidade do vento ao widget da umbrella, e mudar a recomendação quando o vento for muito forte (guarda-chuva pode ser arrastado/pegar invertido).

## 📋 Análise do Código Atual

### Estrutura do Widget (`src/renderer.cpp`)
- **Função**: `drawUmbrellaWidget(int x, int y, const owm_hourly_t *hourly, int hours, int64_t current_dt)`
- **Linha**: ~933
- **Estados atuais**:
  1. `maxPop < 0.30`: Sem chuva → X sobre umbrella
  2. `0.30 <= maxPop < 0.70`: Chuva futura → Umbrella fechada
  3. `maxPop >= 0.70`: Chuva iminente → Umbrella aberta

### Dados Disponíveis
```cpp
// api_response.h - owm_hourly_t
float wind_speed;       // m/s (padrão)
float wind_gust;        // m/s (rajada)
int   wind_deg;         // direção em graus
```

## 🎨 Novos Estados Propostos

### Lógica de Decisão:
```
SE (maxPop < 0.30):
    → Sem chuva (X sobre umbrella)

SENÃO SE (wind_speed > LIMITE_VENTO_FORTE):
    → Muito vento! (ícone de vento + "Too windy")
    
SENÃO SE (maxPop < 0.70):
    → Chuva futura (umbrella fechada)

SENÃO:
    → Chuva iminente (umbrella aberta)
```

### Limite de Vento:
| Unidade | Valor | Descrição |
|---------|-------|-----------|
| m/s | 10 | ~36 km/h |
| km/h | 36 | ~10 m/s |
| mph | 22 | ~10 m/s |

> **Referência**: Guarda-chuvas comuns aguentam 30-40 km/h. Acima disso, risco de virar.

## 🛠️ Implementação

### Etapa 1: Modificar Assinatura da Função (Opcional)
```cpp
// Opção A: Passar hourly (já tem wind_speed)
void drawUmbrellaWidget(int x, int y, const owm_hourly_t *hourly, int hours, int64_t current_dt)

// Opção B: Passar wind_speed diretamente
void drawUmbrellaWidget(int x, int y, const owm_hourly_t *hourly, int hours, int64_t current_dt, float wind_speed)
```

**Recomendação**: Opção A - usar `hourly[0].wind_speed` (vento atual/próxima hora).

### Etapa 2: Calcular Dados de Vento
```cpp
// Encontrar vento máximo no período analisado
float maxWindSpeed = 0.0f;
float maxWindGust = 0.0f;

for (int i = 0; i < hours && i < HOURLY_GRAPH_MAX; i++) {
  if (hourly[i].wind_speed > maxWindSpeed) {
    maxWindSpeed = hourly[i].wind_speed;
  }
  if (hourly[i].wind_gust > maxWindGust) {
    maxWindGust = hourly[i].wind_gust;
  }
}

// Converter para unidade configurada
#ifdef UNITS_SPEED_KILOMETERSPERHOUR
  maxWindSpeed = maxWindSpeed * 3.6f;  // m/s → km/h
  maxWindGust = maxWindGust * 3.6f;
#endif
#ifdef UNITS_SPEED_MILESPERHOUR
  maxWindSpeed = maxWindSpeed * 2.237f;  // m/s → mph
  maxWindGust = maxWindGust * 2.237f;
#endif
```

### Etapa 3: Novos Estados do Widget

#### Estado: "Too Windy" (Vento Forte)
```cpp
const float WIND_THRESHOLD = 10.0f;  // m/s (~36 km/h)

if (maxPop >= 0.30f && maxWindSpeed > WIND_THRESHOLD) {
  // Ícone: Ventilador, folha soprando, ou umbrella com linhas de vento
  // Texto: "Too windy" ou "Wind: XX km/h"
  
  // Desenhar ícone de vento (usar ícone existente ou criar)
  display.drawInvertedBitmap(x, y + ICON_OFFSET_Y, wi_wind_128x128, 128, 128, GxEPD_BLACK);
  
  // Texto abaixo
  display.setFont(&FONT_8pt8b);
  String windStr = String(static_cast<int>(maxWindSpeed));
  #ifdef UNITS_SPEED_KILOMETERSPERHOUR
    windStr += " km/h";
  #elif UNITS_SPEED_MILESPERHOUR
    windStr += " mph";
  #else
    windStr += " m/s";
  #endif
  
  drawString(centerX, y + TEXT_OFFSET_Y, "Too windy (" + windStr + ")", CENTER);
}
```

### Etapa 4: Textos de Localização

Adicionar em `include/locales/*.inc`:
```cpp
// locale_en_US.inc
#define TXT_TOO_WINDY "Too windy"
#define TXT_WIND_STRONG "Strong wind"

// locale_pt_BR.inc
#define TXT_TOO_WINDY "Muito vento"
#define TXT_WIND_STRONG "Vento forte"
```

## 📁 Arquivos a Modificar

| Arquivo | Mudanças |
|---------|----------|
| `src/renderer.cpp` | Lógica do widget, cálculo de vento |
| `include/_locale.h` | Novas strings de texto |
| `include/locales/*.inc` | Traduções para cada idioma |

## 🎨 Alternativas de UI

### Opção A: Ícone de Vento Separado
- Usar ícone `wi_wind_128x128` existente
- Substituir completamente a umbrella quando vento > limite

### Opção B: Umbrella + Indicador de Vento
- Manter ícone de umbrella
- Adicionar pequeno ícone de vento ou texto "Windy"
- Mudar texto de recomendação

### Opção C: Combo (Recomendada)
- Se vento forte: ícone de vento + "Too windy"
- Senão: lógica atual de umbrella

## 🧪 Testes

### Casos de Teste:
1. **Cenário**: POP=80%, vento=5 m/s → Umbrella aberta
2. **Cenário**: POP=80%, vento=15 m/s → "Too windy"
3. **Cenário**: POP=50%, vento=12 m/s → "Too windy"
4. **Cenário**: POP=20%, vento=20 m/s → X (sem chuva, ignora vento)

### Mockup Data:
```cpp
// Adicionar opções em config.h
#define MOCKUP_WIND_SPEED 15.0f  // m/s
#define MOCKUP_WIND_GUST 20.0f   // m/s
```

## 📅 Cronograma

1. **Etapa 1** (30min): Modificar `renderer.cpp` - lógica de vento
2. **Etapa 2** (20min): Adicionar strings de localização
3. **Etapa 3** (20min): Testar com mockup data
4. **Etapa 4** (10min): Documentar

**Total estimado**: ~80 minutos

## 🔄 Fluxograma Final

```
                    ┌─────────────────┐
                    │    Início       │
                    └────────┬────────┘
                             ▼
                    ┌─────────────────┐
                    │ maxPop < 0.30?  │──Sim──┐
                    └────────┬────────┘       │
                         Não │               │
                             ▼               │
                    ┌─────────────────┐      │
                    │wind_speed>10m/s?│──Sim─┐│
                    └────────┬────────┘      ││
                         Não │              ││
                             ▼              ││
                    ┌─────────────────┐     ││
                    │ maxPop < 0.70?  │─Sim─┼┼─┐
                    └────────┬────────┘     ││  │
                         Não │             ││  │
                             ▼             ││  │
                    ┌─────────────────┐    ││  │
                    │  Umbrella       │    ││  │
                    │  Aberta         │    ││  │
                    └─────────────────┘    ││  │
                                           ││  │
    ┌─────────────────┐  ┌─────────────────┐││  │
    │   X (Sem chuva) │  │ "Too windy"     │││  │
    │                 │  │ + ícone vento   │││  │
    └─────────────────┘  └─────────────────┘││  │
                                           ││  │
                         ┌─────────────────┘│  │
                         │ Umbrella fechada │  │
                         └──────────────────┘  │
                                              │
                              ┌───────────────┘
                              │
                              ▼
                     ┌─────────────────┐
                     │      Fim        │
                     └─────────────────┘
```

---

## ✅ Checklist

- [ ] Modificar `drawUmbrellaWidget()` para calcular vento máximo
- [ ] Adicionar constante `WIND_THRESHOLD` (10 m/s)
- [ ] Criar estado "Too windy" com ícone de vento
- [ ] Adicionar strings de localização em todos os idiomas
- [ ] Testar com mockup data
- [ ] Verificar conversão de unidades
- [ ] Documentar a funcionalidade

---

*Criado em: 2026-03-26*
