# Correções na Status Bar

## Problemas Identificados

### 1. Ícone WiFi Errado
**Atual:** Está usando ícones 32x32 ou 24x24 que parecem distorcidos
**Correto:** Usar ícones 16x16 do projeto original

**Ícones corretos:**
- `wifi_16x16` - Sinal excelente
- `wifi_3_bar_16x16` - Sinal bom  
- `wifi_2_bar_16x16` - Sinal regular
- `wifi_1_bar_16x16` - Sinal fraco
- `wifi_x_16x16` - Sem conexão

### 2. Bateria Deve Usar Ícones da Biblioteca
**Atual:** Retângulo desenhado manualmente
**Correto:** Usar ícones de bateria 24x24 horizontais da biblioteca

**Ícones disponíveis:**
- `battery_full_90deg_24x24` - 100%
- `battery_6_bar_90deg_24x24` - 85%
- `battery_5_bar_90deg_24x24` - 72%
- `battery_4_bar_90deg_24x24` - 58%
- `battery_3_bar_90deg_24x24` - 43%
- `battery_2_bar_90deg_24x24` - 29%
- `battery_1_bar_90deg_24x24` - 15%
- `battery_0_bar_90deg_24x24` - <8%

### 3. Adicionar Texto de Qualidade
**WiFi:** Mostrar "Excellent", "Good", "Fair", "Weak" ou "No Connection"
**Bateria:** Mostrar porcentagem calculada corretamente

## Plano de Correção

### Arquivos a Modificar
1. `src/renderer_ws.cpp` - Corrigir status bar
2. (Opcional) `src/display_utils.cpp` - Já tem funções prontas!

### Mudanças no Código

#### 1. WiFi - Usar ícones 16x16 + texto de qualidade
```cpp
// Função para obter ícone WiFi 16x16
const uint8_t* getWiFiBitmap16(int rssi) {
  if (rssi == 0) return wifi_x_16x16;
  if (rssi >= -50) return wifi_16x16;
  if (rssi >= -60) return wifi_3_bar_16x16;
  if (rssi >= -70) return wifi_2_bar_16x16;
  return wifi_1_bar_16x16;
}

// Texto de qualidade
const char* getWiFiDesc(int rssi) {
  if (rssi == 0) return "No Connection";
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Good";
  if (rssi >= -70) return "Fair";
  return "Weak";
}
```

#### 2. Bateria - Usar ícones 24x24 horizontais
```cpp
// Função similar ao projeto original
const uint8_t* getBatBitmap24(uint32_t batPercent) {
  if (batPercent >= 93) return battery_full_90deg_24x24;
  if (batPercent >= 79) return battery_6_bar_90deg_24x24;
  if (batPercent >= 65) return battery_5_bar_90deg_24x24;
  if (batPercent >= 50) return battery_4_bar_90deg_24x24;
  if (batPercent >= 36) return battery_3_bar_90deg_24x24;
  if (batPercent >= 22) return battery_2_bar_90deg_24x24;
  if (batPercent >= 8) return battery_1_bar_90deg_24x24;
  return battery_0_bar_90deg_24x24;
}
```

#### 3. Layout da Status Bar (compacto, à direita)
```
[↻] 14:32    [📶] Excellent -45dBm    [🔋] 95% (3.85V)
x=550        x=640                  x=760
```

## Implementação

Posso implementar essas correções?

Responda "sim" para prosseguir.
