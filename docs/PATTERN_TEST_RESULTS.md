# E-Paper Pattern Test Results

## Summary

Tests of different pixel patterns for precipitation bars on GxEPD2_750_T7 display (800x480, Black/White).

## ✅ Patterns That WORK

| Rank | Pattern | Code Condition | Density | Visual Quality |
|------|---------|---------------|---------|----------------|
| 🥇 1st | Diagonal strokes | `x%3==0 && (y+x*2)%8<5` | ~20% | ⭐⭐⭐⭐⭐ Best - clean diagonal lines |
| 🥈 2nd | Vertical 75% | `x%2==0 && y%4<3` | 37.5% | ⭐⭐⭐⭐ Good - solid fill |
| 🥉 3rd | Vertical 33% | `x%3==0` | 33% | ⭐⭐⭐ OK - simple lines |
| 4th | Vertical 50% | `x%2==0` | 50% | ⭐⭐⭐ OK - dense lines |

## ❌ Patterns That DON'T WORK

| Pattern | Code | Problem | Result |
|---------|------|---------|--------|
| Horizontal dotted | `y += 2` inside loop | **E-paper hardware bug** | White screen - display fails to render |
| Horizontal lines | Any row skipping pattern | Display controller issue | Partial or no rendering |

## Root Cause

The UC8179 display controller (used in 7.5" e-paper panels) has a known issue with horizontal line skipping patterns. When drawing every Nth row, the display buffer becomes corrupted.

**Safe approach**: Always iterate through all Y values, apply pattern on X axis.

## Recommended Pattern (Current)

```cpp
// Use this for precipitation bars
for (int y = draw_y0; y < draw_y1; y++) {
    for (int x = draw_x0; x < draw_x1; x++) {
        if (x % 3 == 0 && (y + (x * 2)) % 8 < 5) {
            display.drawPixel(x, y, GxEPD_BLACK);
        }
    }
}
```

## Testing Method

1. Draw filled rectangle with pattern
2. Full display refresh
3. Evaluate: visibility, flickering, artifacts
4. Repeat with different patterns
