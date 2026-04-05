# Plano: Vento + Umbrella - Lógica V2

## 🎯 Nova Lógica de Decisão

| POP | Vento | Nível | Estado | Observação |
|-----|-------|-------|--------|------------|
| 0% – 29% | Qualquer | 1 | No rain | Sem chuva relevante |
| 30% – 69% | < 18 km/h | 2 | Compact | Chuva moderada, vento fraco |
| 30% – 69% | ≥ 18 km/h | 3 | Take umbrella | Vento forte exige guarda-chuva normal |
| ≥ 70% | Qualquer | 3 | Take umbrella | Alta probabilidade de chuva |

## 🔄 Diferença da Lógica Anterior

**Antes**: Vento forte → força umbrella fechada (proteção)

**Agora**: Vento forte + chuva moderada → umbrella aberta (necessidade)

> Raciocínio: Se está ventando forte e vai chover, melhor ter a umbrella aberta/pronta do que tentar abrir no meio do vento!

## 🛠️ Implementação

### Pseudocódigo
```cpp
const float WIND_THRESHOLD = 18.0f;  // km/h

// Converter vento para km/h
float windKmh = maxWindSpeed * 3.6f;

// Estado 1: Sem chuva
if (maxPop < 0.30f) {
    // X sobre umbrella
    // Texto: "No rain (X%)"
}
// Estado 3: Take umbrella (2 condições)
else if (maxPop >= 0.70f || windKmh >= WIND_THRESHOLD) {
    // Umbrella aberta
    // Texto: "Take"
}
// Estado 2: Compact (só chega aqui se POP 30-69% E vento < 18 km/h)
else {
    // Umbrella fechada
    // Texto: "Compact"
}
```

### Código Atual (referência)
```cpp
// src/renderer.cpp - linha ~967
// State 1: No rain (POP < 30%)
if (maxPop < 0.30f) {
    // ... draw X ...
}
// State 2: Rain coming later (POP 30-70%)
else if (maxPop < 0.70f) {
    // ... closed umbrella ...
}
// State 3: Umbrella now (POP >= 70%)
else {
    // ... open umbrella ...
}
```

### Novo Código
```cpp
// Constante no topo da função
const float WIND_THRESHOLD_KMH = 18.0f;

// Calcular vento máximo (adicionar antes da lógica de estados)
float maxWindSpeed = 0.0f;
for (int i = 0; i < hours && i < HOURLY_GRAPH_MAX; i++) {
    if (hourly[i].wind_speed > maxWindSpeed) {
        maxWindSpeed = hourly[i].wind_speed;
    }
}
float windKmh = maxWindSpeed * 3.6f;

// State 1: No rain (POP < 30%)
if (maxPop < 0.30f) {
    // Draw umbrella icon + X
    // Text: "No rain (X%)"
}
// State 3: Take umbrella (POP >= 70% OR wind >= 18 km/h)
else if (maxPop >= 0.70f || windKmh >= WIND_THRESHOLD_KMH) {
    // Draw open umbrella icon
    // Text: "Take" or "Rain in Ymin"
}
// State 2: Compact (POP 30-69% AND wind < 18 km/h)
else {
    // Draw closed umbrella icon
    // Text: "Compact" or "Rain in Ymin"
}
```

## 📁 Arquivos a Modificar

| Arquivo | Mudanças |
|---------|----------|
| `src/renderer.cpp` | Lógica de decisão + cálculo de vento |

## 🧪 Casos de Teste

| Cenário | POP | Vento | Estado Esperado |
|---------|-----|-------|-----------------|
| Sem chuva | 20% | 5 km/h | 1 - No rain |
| Sem chuva + vento | 20% | 25 km/h | 1 - No rain |
| Chuva moderada, calmo | 50% | 10 km/h | 2 - Compact |
| Chuva moderada, ventoso | 50% | 20 km/h | 3 - Take |
| Chuva forte, calmo | 80% | 5 km/h | 3 - Take |
| Chuva forte, ventoso | 80% | 30 km/h | 3 - Take |

## 🎨 Visual dos Estados

```
Estado 1 (No rain):       Estado 2 (Compact):      Estado 3 (Take):
    ☂️                        🌂                        ☂️
   ╲╱                        (fechada)                 (aberta)
  "No rain"                 "Compact"                 "Take"
   (20%)                     (50%)                     (80%)
                            
Estado 2 só aparece quando:
- POP entre 30-69%
- VENTO abaixo de 18 km/h
```

## ✅ Checklist

- [ ] Adicionar cálculo de `maxWindSpeed`
- [ ] Adicionar constante `WIND_THRESHOLD_KMH = 18.0f`
- [ ] Modificar condição do Estado 3 para incluir `|| windKmh >= 18`
- [ ] Testar todos os casos de borda

---

*Criado em: 2026-03-26*
