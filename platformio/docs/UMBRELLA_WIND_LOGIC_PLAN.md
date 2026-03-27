# Plano: Vento Influenciando os Estados do Widget de Umbrella

## 🎯 Objetivo
Manter os **3 estados visuais de guarda-chuva**, mas usar a informação de **vento** para escolher qual estado mostrar.

## 📊 Estados Atuais (sem vento)

| Estado | Condição | Ícone | Texto |
|--------|----------|-------|-------|
| 1 - Sem chuva | POP < 30% | Umbrella + X | "No rain (X%)" |
| 2 - Levar fechada | 30% ≤ POP < 70% | Umbrella fechada | "Compact (X%)" ou "Rain in Ymin" |
| 3 - Abrir agora | POP ≥ 70% | Umbrella aberta | "Take (X%)" ou "Rain in Ymin" |

## 🌬️ Nova Lógica com Vento

### Níveis de Vento

| Nível | Velocidade | Descrição |
|-------|------------|-----------|
| Calmo | < 5 m/s (< 18 km/h) | Comportamento normal |
| Moderado | 5-10 m/s (18-36 km/h) | Preferir fechada |
| Forte | > 10 m/s (> 36 km/h) | Guarda-chuva arriscado |

### Tabela de Decisão

| POP | Vento | Estado Escolhido | Racional |
|-----|-------|------------------|----------|
| < 30% | Qualquer | **Estado 1** (X) | Sem chuva |
| 30-70% | Calmo | **Estado 2** (fechada) | Chuva futura, vento OK |
| 30-70% | Moderado | **Estado 2** (fechada) | Vento alto, manter fechada |
| 30-70% | Forte | **Estado 2** (fechada) ⚠️ | "Too windy" - usar com cuidado |
| ≥ 70% | Calmo | **Estado 3** (aberta) | Chuva iminente |
| ≥ 70% | Moderado | **Estado 2** (fechada) | Vento alto, não abrir |
| ≥ 70% | Forte | **Estado 2** (fechada) ⚠️ | Vento muito forte, risco |

### Modificação de Texto

Quando vento for forte (> 10 m/s), adicionar indicador no texto:

```cpp
// Exemplos de texto:
"Compact - Windy"      // POP 50%, vento 12 m/s
"Rain in 30min - Windy" // POP 80%, vento 15 m/s
"Take (80%) - Windy"   // Se decidir mostrar aberta mesmo com vento
```

## 🛠️ Implementação

### Etapa 1: Calcular Vento Máximo
```cpp
void drawUmbrellaWidget(int x, int y, const owm_hourly_t *hourly, 
                        int hours, int64_t current_dt)
{
  // ... código existente para maxPop ...
  
  // NOVO: Calcular vento máximo
  float maxWindSpeed = 0.0f;
  for (int i = 0; i < hours && i < HOURLY_GRAPH_MAX; i++) {
    if (hourly[i].wind_speed > maxWindSpeed) {
      maxWindSpeed = hourly[i].wind_speed;
    }
  }
  
  // Converter para km/h para decisão (mais intuitivo)
  float windKmh = maxWindSpeed * 3.6f;
  
  // Constantes
  const float WIND_MODERATE = 18.0f;  // 5 m/s
  const float WIND_STRONG = 36.0f;    // 10 m/s
  
  bool isWindy = (windKmh > WIND_STRONG);
  bool isModerateWind = (windKmh > WIND_MODERATE && windKmh <= WIND_STRONG);
  
  // ... resto da lógica ...
}
```

### Etapa 2: Lógica de Estado Modificada
```cpp
// Estado 1: Sem chuva (sem mudança)
if (maxPop < 0.30f) {
  // ... código existente ...
}

// Estados 2 e 3: Com chuva - considerar vento
else {
  // Decidir estado baseado em POP e vento
  bool shouldOpen = (maxPop >= 0.70f) && (windKmh <= WIND_STRONG);
  
  if (shouldOpen) {
    // Estado 3: Umbrella aberta
    // ... código existente ...
    
    // Adicionar aviso de vento se moderado
    if (isModerateWind) {
      // Texto: "Take - Breezy"
    } else {
      // Texto: "Take"
    }
  } 
  else {
    // Estado 2: Umbrella fechada
    // ... código existente ...
    
    // Modificar texto se vento forte
    if (isWindy) {
      // Texto: "Compact - Windy" ou "Rain in Xmin - Windy"
    } else if (isModerateWind) {
      // Texto: "Compact - Breezy"
    }
  }
}
```

### Etapa 3: Strings de Localização

Adicionar em `include/locales/*.inc`:

```cpp
// locale_en_US.inc
#define TXT_WINDY "Windy"
#define TXT_BREEZY "Breezy"

// locale_pt_BR.inc  
#define TXT_WINDY "Ventando"
#define TXT_BREEZY "Ventinho"
```

## 📁 Arquivos a Modificar

| Arquivo | Mudanças |
|---------|----------|
| `src/renderer.cpp` | Lógica de decisão com vento |
| `include/_locale.h` | Strings "Windy", "Breezy" |
| `include/locales/*.inc` | Traduções |

## 🧪 Casos de Teste

| Cenário | POP | Vento | Resultado Esperado |
|---------|-----|-------|-------------------|
| Chuva iminente, calmo | 80% | 3 m/s | Estado 3 (aberta) |
| Chuva iminente, ventoso | 80% | 15 m/s | Estado 2 (fechada) + "Windy" |
| Chuva futura, calmo | 50% | 2 m/s | Estado 2 (fechada) |
| Chuva futura, ventoso | 50% | 12 m/s | Estado 2 (fechada) + "Windy" |
| Sem chuva, ventoso | 10% | 20 m/s | Estado 1 (X) - ignora vento |

## 🎨 Vantagens desta Abordagem

1. **Sem novos ícones** - Reusa os 3 existentes
2. **Simples** - Só muda a lógica de decisão
3. **Informativo** - Texto indica condição de vento
4. **Útil** - Alerta quando guarda-chuva pode não funcionar bem

## 🔄 Fluxograma

```
┌─────────────────┐
│    Início       │
└────────┬────────┘
         ▼
┌─────────────────┐
│ maxPop < 0.30?  │────Sim────┐
└────────┬────────┘           │
      Não │                   │
         ▼                   │
┌─────────────────┐          │
│ windKmh > 36?   │───Sim───┐│  // Vento forte
└────────┬────────┘         ││
      Não │                 ││
         ▼                  ││
┌─────────────────┐         ││
│ maxPop >= 0.70? │─Sim─┐   ││
└────────┬────────┘     │   ││
      Não │             │   ││
         ▼              ▼   ▼│
    ┌────────────┐ ┌────────────┐
    │  Estado 2  │ │  Estado 2  │
    │ (fechada)  │ │ (fechada)  │
    │  normal    │ │  + Windy   │
    └────────────┘ └────────────┘
                           ▲
    ┌────────────┐         │
    │  Estado 3  │─────────┘
    │  (aberta)  │   // Vento forte força fechada
    └────────────┘
         ▲
         │
    ┌────────────┐
    │  Estado 1  │
    │   (X)      │
    │  No rain   │
    └────────────┘
```

## ✅ Checklist

- [ ] Calcular vento máximo no período
- [ ] Adicionar constantes de limite (18 e 36 km/h)
- [ ] Modificar lógica de decisão Estado 2 vs 3
- [ ] Adicionar sufixo "Windy" quando vento > 36 km/h
- [ ] Adicionar sufixo "Breezy" quando vento 18-36 km/h
- [ ] Adicionar strings de localização
- [ ] Testar casos de borda

---

*Revisado em: 2026-03-26*
