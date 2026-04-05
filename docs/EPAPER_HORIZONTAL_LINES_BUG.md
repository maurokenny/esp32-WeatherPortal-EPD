# E-Paper Display Bug: Horizontal Line Skipping Pattern Causes White Screen

## Issue Summary

When drawing precipitation bars using a horizontal dotted pattern (`y += 2` row skipping), the e-paper display renders as a completely white screen. However, a vertical dotted pattern (`x += 2` column skipping) works correctly.

## Hardware/Software Context

- **Display**: GxEPD2_750_T7 (800x480, Black/White)
- **Driver Library**: GxEPD2 v1.6.5
- **Controller**: Unknown (likely UC8179 or similar)
- **Framework**: Arduino ESP32

## Reproduction

### Code that FAILS (White Screen)
```cpp
// Horizontal dotted pattern - BROKEN
for (int y = draw_y0; y < draw_y1; y += 2) {  // DON'T USE
  for (int x = draw_x0; x < draw_x1; x++) {
    display.drawPixel(x, y, GxEPD_BLACK);
  }
}
```

### Code that WORKS
```cpp
// Vertical dotted pattern - WORKS
for (int y = draw_y0; y < draw_y1; y++) {
  for (int x = draw_x0; x < draw_x1; x += 2) {  // OK
    display.drawPixel(x, y, GxEPD_BLACK);
  }
}

// Solid fill - WORKS
for (int y = draw_y0; y < draw_y1; y++) {
  for (int x = draw_x0; x < draw_x1; x++) {
    display.drawPixel(x, y, GxEPD_BLACK);
  }
}
```

## Symptoms

1. **Full white screen**: No image renders at all
2. **Happens consistently**: Not intermittent, 100% reproducible
3. **Silent failure**: No crash, no exception, code continues running
4. **Display refresh appears to complete**: The e-paper goes through its update cycle (flashing), but ends up all white

## Root Cause Hypothesis

The e-paper controller likely processes data in scanlines (horizontal rows). When pixels are written with gaps in the Y direction (skipping every other row), the controller's internal buffer or state machine may become corrupted because:

1. **Partial scanline updates**: The controller expects contiguous row data
2. **Buffer alignment**: Row-based buffers don't handle sparse Y writes well
3. **SPI/command timing**: The controller interprets skipped rows as end-of-frame or other control signals

The fact that X-skipping works suggests the controller handles sparse column data within a row correctly (probably because it's within the same scanline buffer).

## Workarounds

### Option 1: Solid Bars (Current in feature/openmeteo-pop-graph)
```cpp
for (int y = draw_y0; y < draw_y1; y++) {
  for (int x = draw_x0; x < draw_x1; x++) {
    display.drawPixel(x, y, GxEPD_BLACK);
  }
}
```
- **Pros**: Cleanest visual, works reliably
- **Cons**: More ink coverage = slower refresh, potentially more power draw

### Option 2: Vertical Dotted Pattern (Current in feature/openmeteo-vertical-lines)
```cpp
for (int y = draw_y0; y < draw_y1; y++) {
  for (int x = draw_x0; x < draw_x1; x += 2) {
    display.drawPixel(x, y, GxEPD_BLACK);
  }
}
```
- **Pros**: Uses less ink, faster refresh
- **Cons**: Visual pattern is vertical lines instead of horizontal dots

### Option 3: Checkerboard Pattern (NOT TESTED - May Fail)
```cpp
for (int y = draw_y0; y < draw_y1; y++) {
  for (int x = draw_x0; x < draw_x1; x++) {
    if ((x + y) % 2 == 0) {
      display.drawPixel(x, y, GxEPD_BLACK);
    }
  }
}
```
- Unknown if this would work - depends on whether the controller can handle alternating pixels

## Investigation Leads for Future Debugging

1. **Check GxEPD2 source**: Look at how `drawPixel()` handles coordinate translation
   - File: `GxEPD2_EPD.cpp` or display-specific driver
   - Look for `writePixel()`, `writeFillRect()`, or similar

2. **Try GxEPD2 native methods**:
   ```cpp
   display.writeFillRect(x, y, w, h, GxEPD_BLACK);
   ```
   Instead of manual pixel loops. The native methods may use different buffer strategies.

3. **Test with GxEPD2 `writeImage()`**:
   - Create a 1-bit bitmap in RAM
   - Use `writeImage()` or `drawBitmap()` to push it to display
   - Bypasses per-pixel coordinate translation

4. **Check window setting**:
   ```cpp
   display.setPartialWindow(x, y, w, h);
   ```
   Before drawing. May affect how the controller handles sparse writes.

5. **Verify SPI transaction settings**:
   - Check if `SPI.beginTransaction()` settings differ between working/failing cases
   - Some displays are sensitive to SPI speed/mode during partial updates

6. **Test with full refresh vs partial**:
   ```cpp
   display.display(false);  // full refresh
   vs
   display.display(true);   // partial refresh
   ```

7. **Check display datasheet**:
   - UC8179 or similar controller datasheet
   - Look for "gate scan", "source output", or "partial window" constraints

## Related Code Locations

- `src/renderer.cpp`: `drawOutlookGraph()` function
- See lines around `#ifdef ENABLE_HOURLY_PRECIP_GRAPH`

## Related Branches

- `feature/openmeteo-vertical-lines`: Uses working vertical dotted pattern
- `feature/openmeteo-pop-graph`: Uses solid bars (this branch)

## Next Steps for Investigation

1. Test if `writeFillRect()` with a pattern brush works
2. Create a minimal test case that reproduces the issue
3. Check if the issue persists with other GxEPD2 display classes
4. Look at GxEPD2 issue tracker for similar reports
