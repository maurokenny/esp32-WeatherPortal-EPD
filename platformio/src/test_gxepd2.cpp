/* Test GxEPD2 Display
 * Simple test using existing display object from renderer.cpp
 */

#include <Arduino.h>
#include "renderer.h"
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>

void runDisplayTest() {
  Serial.println("=== GxEPD2 Display Test ===");
  
  // Init display
  initDisplay();
  
  Serial.print("Display size: ");
  Serial.print(display.width());
  Serial.print("x");
  Serial.println(display.height());
  
  // Draw test pattern
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Title
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(50, 50);
    display.println("GxEPD2 Test!");
    
    // Lines
    display.drawLine(50, 70, 750, 70, GxEPD_BLACK);
    display.drawLine(50, 410, 750, 410, GxEPD_BLACK);
    
    // Rectangle outline
    display.drawRect(100, 100, 200, 100, GxEPD_BLACK);
    display.setFont(&FreeMono9pt7b);
    display.setCursor(120, 150);
    display.println("Rectangle");
    
    // Filled rectangle
    display.fillRect(500, 100, 200, 100, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(520, 150);
    display.println("Filled");
    
    // Circle
    display.setTextColor(GxEPD_BLACK);
    display.fillCircle(200, 300, 50, GxEPD_WHITE);
    display.drawCircle(200, 300, 50, GxEPD_BLACK);
    display.setCursor(175, 305);
    display.println("Circle");
    
    // Bottom text
    display.setFont(&FreeMono12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(200, 450);
    display.println("FireBeetle 2 + 7.5\" E-Paper");
    
  } while (display.nextPage());
  
  Serial.println("Test pattern displayed!");
  powerOffDisplay();
}
