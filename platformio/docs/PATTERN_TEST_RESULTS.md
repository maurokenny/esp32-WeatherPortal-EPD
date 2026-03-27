# Resultados dos Testes de Padrões de Dithering E-Paper

## Resumo Executivo

**Melhor padrão encontrado:** `Option 8` - Traços diagonais deslocados

```cpp
if (x % 3 == 0 && (y + (x * 2)) % 8 < 5)
```

---

## 🏆 Ranking dos Padrões

| Rank | Padrão | Condição | Avaliação | Densidade |
|------|--------|----------|-----------|-----------|
| 🥇 | **Traços Diagonais** | `x%3==0 && (y+x*2)%8<5` | ⭐⭐⭐⭐⭐ **BEST** | ~20% |
| 🥈 | Linhas Verticais 75% | `x%2==0 && y%4<3` | ⭐⭐⭐⭐ Bom | 37.5% |
| 🥉 | Linhas Verticais 33% | `x%3==0` | ⭐⭐⭐ Funciona | 33% |
| 4 | Linhas Verticais 50% | `x%2==0` | ⭐⭐⭐ Funciona | 50% |
| 5 | Linhas Verticais 70% | `x%3==0 && y%10<7` | ⭐⭐ Funciona, feio | 23% |

---

## ✅ Padrões Funcionais

### 🏆 Opção 8: Traços Diagonais Deslocados (ATIVA)
```cpp
for (int y = draw_y0; y < draw_y1; y++) {
  for (int x = draw_x0; x < draw_x1; x++) {
    if (x % 3 == 0) {
      if ((y + (x * 2)) % 8 < 5) {
        display.drawPixel(x, y, GxEPD_BLACK);
      }
    }
  }
}
```
**Visual:**
```
■ □ □ ■ □ □ ■ □ □
■ □ □ ■ □ □ ■ □ □
■ □ □ ■ □ □ ■ □ □
■ □ □ ■ □ □ □ □ □
■ □ □ ■ □ □ □ □ □
□ □ □ □ □ □ □ □ ■
□ □ □ □ □ □ □ □ ■
□ □ □ □ □ □ □ □ ■
(deslocamento diagonal cria padrão visual interessante)
```
**Por que funciona:**
- Usa `x % 3 == 0` para espaçamento horizontal
- Traços de 5px contíguos `(y + offset) % 8 < 5`
- Offset `(x * 2)` cria deslocamento diagonal por coluna
- Combina contiguidade em Y com variação visual

### Opção C: Linhas Verticais 75%
```cpp
if (x % 2 == 0 && y % 4 < 3)
```
**Visual:**
```
■ □ ■ □ ■ □ ■ □ ■
■ □ ■ □ ■ □ ■ □ ■
■ □ ■ □ ■ □ ■ □ ■
□ □ □ □ □ □ □ □ □  (gap)
```

### Opção 1: Linhas Verticais Simples
```cpp
if (x % 3 == 0)
```
**Visual:**
```
■ □ □ ■ □ □ ■ □ □
■ □ □ ■ □ □ ■ □ □
■ □ □ ■ □ □ ■ □ □
```

---

## ❌ Padrões que NÃO Funcionam (White Screen)

| Padrão | Condição | Problema |
|--------|----------|----------|
| Xadrez tradicional | `(x + y) % 2 == 0` | Alternância em Y muito rápida |
| Pontos 2x2 | `(x%2==0) && (y%2==0)` | Alternância em Y |
| Blocos 2x2 | `((x/2)+(y/2))%2==0` | Alternância em Y |
| Diagonal | `(x + y) % 4 == 0` | Alternância em Y |
| Cruz diagonal | `(x+y)%4==0 \|\| (x-y)%4==0` | Alternância em Y |
| Fileiras deslocadas | `y%2? x%4==0 : (x+2)%4==0` | Alternância em Y |
| Duas etapas xadrez | Etapas separadas por Y | Ainda alterna em Y |

---

## 🔬 Regras Descobertas

### ❌ O que NÃO funciona:
```cpp
// Qualquer padrão que ALTERNAR a cada linha (y % 2)
y % 2 == 0
(x + y) % 2 == 0
```

### ✅ O que FUNCIONA:
```cpp
// Blocos de pelo menos 3-5 linhas contíguas
y % 4 < 3   // 3 linhas ON, 1 OFF ✓
y % 8 < 5   // 5 linhas ON, 3 OFF ✓
y % 10 < 7  // 7 linhas ON, 3 OFF ✓

// Combinado com espaçamento em X
x % 2 == 0 && y % 4 < 3
x % 3 == 0 && (y + x*2) % 8 < 5  // com offset diagonal
```

### 📊 Regra de Ouro:
> **O controlador aceita gaps em Y desde que sejam blocos de pelo menos 3-5 linhas contíguas. Alternância a cada linha (y%2) corrompe o buffer.**

---

## Hardware Testado

- **Display:** GxEPD2_750_T7 (800x480, Black/White)
- **Driver Board:** WaveShare Rev 2.3
- **Biblioteca:** GxEPD2 v1.6.5
- **Data:** 2026-03-26

---

## Código Ativo em `renderer.cpp`

```cpp
// graph Precipitation
#ifdef ENABLE_HOURLY_PRECIP_GRAPH
  // ... bounds checking ...
  
  // Option 8: Diagonal dashed lines (BEST - aesthetic diagonal pattern)
  for (int y = draw_y0; y < draw_y1; y++) {
    for (int x = draw_x0; x < draw_x1; x++) {
      if (x % 3 == 0) {
        if ((y + (x * 2)) % 8 < 5) {
          display.drawPixel(x, y, GxEPD_BLACK);
        }
      }
    }
  }
#endif
```

---

*Última atualização: 2026-03-26*
*Melhor padrão: Traços Diagonais Deslocados*
