# Plano de Correção de Bugs - Weather Display

## 🐛 Problemas Identificados

### 1. Ícone Principal (196x196) Distorcido
**Prioridade: ALTA**

**Problema:** O ícone de clima grande está sendo renderizado incorretamente, parecendo distorcido ou com pixels errados.

**Causa provável:** 
- A função `drawBitmap()` está interpretando incorretamente o formato dos dados
- Os ícones da biblioteca esp32-weather-epd-assets podem ter um formato diferente do esperado
- Alinhamento de bytes (byte alignment) pode estar incorreto

**Solução:**
- [ ] Verificar formato real dos bitmaps na biblioteca (XBM, BMP raw, etc.)
- [ ] Reescrever função `drawBitmap()` para formato correto
- [ ] Testar com ícone simples (quadrado preenchido) para validar
- [ ] Usar função nativa do Waveshare se disponível (`Paint_DrawBitMap()`)

**Arquivos:** `src/renderer_ws.cpp` - função `drawBitmap()`

---

### 2. Texto "updated" Sobrepõe Ícones
**Prioridade: ALTA**

**Problema:** O texto "Updated HH:MM" na status bar está sobrepondo outros elementos visuais.

**Causa provável:**
- Posicionamento Y incorreto (muito para cima)
- Falta de área de clipping ou margem

**Solução:**
- [ ] Ajustar posição Y da status bar de `DISP_HEIGHT - 32` para `DISP_HEIGHT - 28`
- [ ] Reduzir altura da status bar para 24px
- [ ] Verificar se há espaço suficiente entre o gráfico e a status bar
- [ ] Limpar área da status bar antes de desenhar (Paint_ClearWindows)

**Arquivos:** `src/renderer_ws.cpp` - função `drawStatusBar()`

---

### 3. Valores Muito Pequenos
**Prioridade: ALTA**

**Problema:** Os números (temperatura, valores dos widgets) estão difíceis de ler.

**Causa provável:**
- Fontes muito pequenas para display E-Paper de 7.5"
- Falta de contraste adequado

**Solução:**
- [ ] Aumentar fonte da temperatura principal: Font24 → Font20 ou usar fonte bold
- [ ] Aumentar fonte dos valores dos widgets: FONT_12PT → FONT_14PT
- [ ] Aumentar fonte dos labels dos widgets: FONT_7PT → FONT_8PT
- [ ] Considerar usar fontes customizadas maiores (Montserrat_48pt8b_temperature)
- [ ] Adicionar espaçamento entre caracteres se necessário

**Arquivos:** `src/renderer_ws.cpp` - funções `drawCurrentConditions()`, `drawWidget()`

---

### 4. Cidade e Data Sobrepõem Ícones
**Prioridade: ALTA**

**Problema:** "Lille" e a data estão sobrepondo os ícones da previsão de 5 dias.

**Causa provável:**
- Posicionamento X incorreto (muito à esquerda para alinhamento RIGHT)
- Falta de margem direita
- Cálculo de largura do texto incorreto

**Solução:**
- [ ] Verificar posição X atual (798) - deve ser 780 para margem de 20px
- [ ] Garantir que alinhamento RIGHT funcione corretamente
- [ ] Limpar área do header antes de desenhar
- [ ] Adicionar margem de segurança de 10px
- [ ] Verificar se string da cidade não está muito longa (truncar se necessário)

**Arquivos:** `src/renderer_ws.cpp` - função `drawLocationDate()`

---

### 5. Previsão de 5 Dias com Mesmo Dia da Semana
**Prioridade: ALTA**

**Problema:** Todos os 5 ícones de previsão mostram "Mon", "Mon", "Mon" ao invés de "Mon", "Tue", "Wed", etc.

**Causa provável:**
- Bug no cálculo do timestamp para cada dia
- Todos os `daily[i].dt` estão recebendo o mesmo valor
- Problema no parsing da API Open-Meteo

**Solução:**
- [ ] Debugar valores de `daily[i].dt` no serial
- [ ] Verificar se Open-Meteo retorna timestamps corretos para cada dia
- [ ] Se necessário, calcular timestamps manualmente: `today + (i * 86400)`
- [ ] Verificar fuso horário na conversão
- [ ] Testar com `localtime()` vs `gmtime()`

**Arquivos:** `src/main.cpp` - função `fetchWeatherData()`
`src/renderer_ws.cpp` - função `drawForecast()`

---

## 📋 Ordem de Implementação

### Fase A - Correções Críticas (Hoje)
1. Corrigir bug dos 5 dias (mais fácil, impacta funcionalidade)
2. Corrigir sobreposição cidade/data (ajuste simples de coordenadas)
3. Corrigir sobreposição "updated" (ajuste de coordenadas)

### Fase B - Melhorias Visuais (Amanhã)
4. Aumentar tamanho das fontes
5. Corrigir renderização do ícone principal (pode ser complexo)

---

## 🧪 Testes Necessários

- [ ] Verificar se ícone 196x196 renderiza sem distorção
- [ ] Verificar se 5 dias mostram dias sequenciais corretos
- [ ] Verificar se não há sobreposição de elementos
- [ ] Verificar legibilidade das fontes aumentadas
- [ ] Testar com diferentes tamanhos de cidade (curta, média, longa)

---

## 📝 Notas Técnicas

### Formato de Bitmap Waveshare
Os ícones da biblioteca são arrays de bytes no formato:
```c
const unsigned char icon_name[] = {
  0x00, 0x00, ... // 1 bit per pixel, 0=black, 1=white (or inverted)
};
```

Tamanho esperado: `(width * height) / 8` bytes

Para 196x196: `(196 * 196) / 8 = 4802` bytes

Possíveis problemas:
1. Ordem dos bits (MSB vs LSB)
2. Padding/align de linhas (múltiplos de 8)
3. Inversão de cores (0=black vs 0=white)

### Posições Corretas Sugeridas

```
Layout 800x480:

Cabeçalho (y=0 a 60):
  - Cidade: x=780, y=23, align=RIGHT
  - Data: x=780, y=51, align=RIGHT

Área Principal (y=60 a 430):
  - Ícone 196x196: x=0, y=0
  - Temperatura: x=278, y=100
  - Widget grid: y=204 a 460
  - Forecast: x=398 a 798, y=60 a 140
  - Gráfico: x=350 a 780, y=216 a 400

Status Bar (y=430 a 480):
  - y_base = 456 (DISP_HEIGHT - 24)
  - Margem inferior = 8px
```

---

## 💾 Estimativa de Mudanças

- `renderer_ws.cpp`: ~150 linhas modificadas
- `main.cpp`: ~30 linhas modificadas
- `renderer.h`: nenhuma mudança esperada
