"""
Mock Server for ESP32 Weather EPD Blackbox Testing
Architecture: Router → Scenarios → Validator (Pattern Matching)

Components:
- Router: FastAPI endpoint routing
- Scenarios: Test scenario data store (Sunny, Rainy, Storm, etc.)
- Validator: Pattern matching engine for render events

Endpoints:
- GET  /v1/forecast        → Returns Open-Meteo JSON for current scenario
- POST /test/scenario/:name → Sets active scenario
- POST /test/events        ← Receives captured render events from firmware
- POST /test/validate      → Validates captured events against patterns
- GET  /test/report        → Returns validation report
"""

from fastapi import FastAPI, HTTPException, Path
from pydantic import BaseModel, Field
from typing import Dict, List, Any, Optional
from enum import Enum
import uvicorn
from datetime import datetime

# ============================================================================
# Data Models
# ============================================================================


class RenderEvent(BaseModel):
    """Event captured by Render Mock and sent from firmware"""

    event_type: str = Field(
        ..., description="Type: icon_drawn, umbrella_drawn, text_drawn"
    )
    icon_name: Optional[str] = Field(None, description="Icon identifier")
    x: Optional[int] = None
    y: Optional[int] = None
    weather_code: Optional[int] = None
    pop: Optional[int] = Field(None, description="Precipitation probability %")
    precipitation: Optional[float] = None
    timestamp: Optional[str] = None


class ValidationRequest(BaseModel):
    """Request to trigger validation"""

    scenario: str
    expected_patterns: Optional[List[str]] = None


class ValidationResult(BaseModel):
    """Validation result with pattern matching details"""

    scenario: str
    valid: bool
    matched_patterns: List[str]
    missing_patterns: List[str]
    unexpected_events: List[str]
    captured_events_count: int
    message: str
    details: Optional[Dict[str, Any]] = None


class TestReport(BaseModel):
    """Full test report"""

    current_scenario: str
    captured_events: List[RenderEvent]
    last_validation: Optional[ValidationResult] = None
    total_events_captured: int
    server_uptime: str


# ============================================================================
# Scenarios Module
# ============================================================================


class ScenarioStore:
    """Stores and manages test scenarios with Open-Meteo data"""

    SCENARIOS: Dict[str, Dict[str, Any]] = {
        "sunny": {
            "description": "Clear sunny day, no precipitation",
            "expected_patterns": ["icon_sun", "no_umbrella", "temperature_warm", "humidity_low", "wind_calm"],
            "openmeteo": {
                "latitude": 51.5074,
                "longitude": -0.1278,
                "timezone": "Europe/London",
                "hourly": {
                    "time": [
                        "2025-01-15T12:00",
                        "2025-01-15T13:00",
                        "2025-01-15T14:00",
                    ],
                    "temperature_2m": [22.0, 23.0, 24.0],
                    "relative_humidity_2m": [45, 42, 40],
                    "precipitation_probability": [0, 0, 0],
                    "precipitation": [0.0, 0.0, 0.0],
                    "rain": [0.0, 0.0, 0.0],
                    "showers": [0.0, 0.0, 0.0],
                    "weather_code": [0, 0, 0],
                    "wind_speed_10m": [5.0, 6.0, 5.5],
                    "surface_pressure": [1015.0, 1016.0, 1017.0],
                    "visibility": [10.0, 10.0, 10.0],
                },
                "daily": {
                    "time": ["2025-01-15", "2025-01-16"],
                    "weather_code": [0, 1],
                    "temperature_2m_max": [24.0, 22.0],
                    "temperature_2m_min": [15.0, 14.0],
                    "precipitation_probability_max": [0, 10],
                },
            },
        },
        "light_rain": {
            "description": "Light rain with moderate precipitation",
            "expected_patterns": ["icon_rain", "umbrella_drawn", "pop_above_threshold", "temperature_mild", "humidity_drawn", "wind_drawn"],
            "openmeteo": {
                "latitude": 51.5074,
                "longitude": -0.1278,
                "timezone": "Europe/London",
                "hourly": {
                    "time": [
                        "2025-01-15T12:00",
                        "2025-01-15T13:00",
                        "2025-01-15T14:00",
                    ],
                    "temperature_2m": [18.0, 17.5, 17.0],
                    "relative_humidity_2m": [75, 78, 80],
                    "precipitation_probability": [30, 45, 60],
                    "precipitation": [0.5, 1.2, 2.0],
                    "rain": [0.5, 1.2, 2.0],
                    "showers": [0.0, 0.0, 0.0],
                    "weather_code": [61, 61, 63],
                    "wind_speed_10m": [8.0, 9.0, 10.0],
                    "surface_pressure": [1005.0, 1003.0, 1002.0],
                    "visibility": [5.0, 4.0, 3.0],
                },
                "daily": {
                    "time": ["2025-01-15", "2025-01-16"],
                    "weather_code": [61, 80],
                    "temperature_2m_max": [18.0, 16.0],
                    "temperature_2m_min": [12.0, 11.0],
                    "precipitation_probability_max": [60, 80],
                },
            },
        },
        "heavy_storm": {
            "description": "Heavy storm with thunder and high precipitation",
            "expected_patterns": [
                "icon_thunder",
                "umbrella_drawn",
                "high_precipitation",
                "temperature_mild",
                "humidity_high",
                "wind_strong",
            ],
            "openmeteo": {
                "latitude": 51.5074,
                "longitude": -0.1278,
                "timezone": "Europe/London",
                "hourly": {
                    "time": [
                        "2025-01-15T12:00",
                        "2025-01-15T13:00",
                        "2025-01-15T14:00",
                    ],
                    "temperature_2m": [16.0, 15.5, 15.0],
                    "relative_humidity_2m": [90, 92, 95],
                    "precipitation_probability": [90, 95, 100],
                    "precipitation": [15.0, 25.0, 30.0],
                    "rain": [10.0, 15.0, 20.0],
                    "showers": [5.0, 10.0, 10.0],
                    "weather_code": [95, 95, 96],
                    "wind_speed_10m": [25.0, 35.0, 40.0],
                    "surface_pressure": [995.0, 990.0, 988.0],
                    "visibility": [2.0, 1.0, 1.0],
                },
                "daily": {
                    "time": ["2025-01-15", "2025-01-16"],
                    "weather_code": [95, 95],
                    "temperature_2m_max": [16.0, 14.0],
                    "temperature_2m_min": [10.0, 9.0],
                    "precipitation_probability_max": [100, 90],
                },
            },
        },
        "high_precipitation_prob": {
            "description": "High probability of rain (>50%) but no current precipitation",
            "expected_patterns": ["icon_cloud", "umbrella_drawn", "pop_high_no_precip", "temperature_mild", "humidity_drawn"],
            "openmeteo": {
                "latitude": 51.5074,
                "longitude": -0.1278,
                "timezone": "Europe/London",
                "hourly": {
                    "time": [
                        "2025-01-15T12:00",
                        "2025-01-15T13:00",
                        "2025-01-15T14:00",
                    ],
                    "temperature_2m": [20.0, 19.5, 19.0],
                    "relative_humidity_2m": [70, 75, 80],
                    "precipitation_probability": [70, 80, 85],
                    "precipitation": [0.0, 0.0, 0.0],
                    "rain": [0.0, 0.0, 0.0],
                    "showers": [0.0, 0.0, 0.0],
                    "weather_code": [3, 3, 61],
                    "wind_speed_10m": [10.0, 12.0, 15.0],
                    "surface_pressure": [1010.0, 1008.0, 1006.0],
                    "visibility": [8.0, 7.0, 5.0],
                },
                "daily": {
                    "time": ["2025-01-15", "2025-01-16"],
                    "weather_code": [3, 61],
                    "temperature_2m_max": [20.0, 18.0],
                    "temperature_2m_min": [14.0, 13.0],
                    "precipitation_probability_max": [85, 90],
                },
            },
        },
        "no_precip_low_prob": {
            "description": "Cloudy with low precipitation probability (<30%)",
            "expected_patterns": ["icon_cloud", "no_umbrella", "pop_low", "temperature_mild", "humidity_drawn", "wind_calm"],
            "openmeteo": {
                "latitude": 51.5074,
                "longitude": -0.1278,
                "timezone": "Europe/London",
                "hourly": {
                    "time": [
                        "2025-01-15T12:00",
                        "2025-01-15T13:00",
                        "2025-01-15T14:00",
                    ],
                    "temperature_2m": [21.0, 22.0, 23.0],
                    "relative_humidity_2m": [50, 48, 45],
                    "precipitation_probability": [10, 15, 20],
                    "precipitation": [0.0, 0.0, 0.0],
                    "rain": [0.0, 0.0, 0.0],
                    "showers": [0.0, 0.0, 0.0],
                    "weather_code": [2, 2, 2],
                    "wind_speed_10m": [5.0, 6.0, 5.0],
                    "surface_pressure": [1018.0, 1019.0, 1020.0],
                    "visibility": [10.0, 10.0, 10.0],
                },
                "daily": {
                    "time": ["2025-01-15", "2025-01-16"],
                    "weather_code": [2, 2],
                    "temperature_2m_max": [23.0, 22.0],
                    "temperature_2m_min": [15.0, 14.0],
                    "precipitation_probability_max": [20, 25],
                },
            },
        },
        "snowy": {
            "description": "Snowy conditions",
            "expected_patterns": ["icon_snow", "umbrella_drawn", "temperature_freezing", "humidity_high", "wind_drawn"],
            "openmeteo": {
                "latitude": 51.5074,
                "longitude": -0.1278,
                "timezone": "Europe/London",
                "hourly": {
                    "time": [
                        "2025-01-15T12:00",
                        "2025-01-15T13:00",
                        "2025-01-15T14:00",
                    ],
                    "temperature_2m": [0.0, -1.0, -1.5],
                    "relative_humidity_2m": [85, 88, 90],
                    "precipitation_probability": [80, 85, 90],
                    "precipitation": [2.0, 3.0, 4.0],
                    "rain": [0.0, 0.0, 0.0],
                    "showers": [0.0, 0.0, 0.0],
                    "snowfall": [2.0, 3.0, 4.0],
                    "weather_code": [71, 73, 75],
                    "wind_speed_10m": [10.0, 12.0, 15.0],
                    "surface_pressure": [1020.0, 1022.0, 1024.0],
                    "visibility": [3.0, 2.0, 2.0],
                },
                "daily": {
                    "time": ["2025-01-15", "2025-01-16"],
                    "weather_code": [71, 73],
                    "temperature_2m_max": [0.0, -2.0],
                    "temperature_2m_min": [-5.0, -6.0],
                    "precipitation_probability_max": [90, 85],
                },
            },
        },
    }

    def __init__(self):
        self.current_scenario: str = "sunny"
        self.captured_events: List[RenderEvent] = []
        self.last_validation: Optional[ValidationResult] = None
        self.start_time = datetime.now()

    def get_scenario(self, name: Optional[str] = None) -> Dict[str, Any]:
        """Get scenario data by name (or current if None)"""
        target = name or self.current_scenario
        if target not in self.SCENARIOS:
            raise HTTPException(
                status_code=404, detail=f"Scenario '{target}' not found"
            )
        return self.SCENARIOS[target]

    def set_scenario(self, name: str) -> bool:
        """Set current scenario and reset capture buffer"""
        if name not in self.SCENARIOS:
            available = ", ".join(self.SCENARIOS.keys())
            raise HTTPException(
                status_code=400,
                detail=f"Unknown scenario '{name}'. Available: {available}",
            )
        self.current_scenario = name
        self.captured_events = []
        self.last_validation = None
        return True

    def add_event(self, event: RenderEvent) -> None:
        """Add captured render event"""
        if not event.timestamp:
            event.timestamp = datetime.now().isoformat()
        self.captured_events.append(event)

    def get_openmeteo_data(self, name: Optional[str] = None) -> Dict[str, Any]:
        """Get Open-Meteo JSON for scenario"""
        scenario = self.get_scenario(name)
        return scenario.get("openmeteo", {})

    def get_expected_patterns(self, name: Optional[str] = None) -> List[str]:
        """Get expected patterns for scenario"""
        scenario = self.get_scenario(name)
        return scenario.get("expected_patterns", [])


# ============================================================================
# Validator Module (Pattern Matching)
# ============================================================================


class PatternValidator:
    """Validates captured events against expected patterns using pattern matching"""

    # Pattern matching rules
    PATTERN_RULES = {
        # Icon patterns
        "icon_sun": lambda events: any(
            e.event_type == "icon_drawn" and e.icon_name in ["sun", "clear"]
            for e in events
        ),
        "icon_cloud": lambda events: any(
            e.event_type == "icon_drawn"
            and e.icon_name in ["cloud", "overcast", "partly_cloudy"]
            for e in events
        ),
        "icon_rain": lambda events: any(
            e.event_type == "icon_drawn"
            and e.icon_name in ["rain", "light_rain", "moderate_rain"]
            for e in events
        ),
        "icon_thunder": lambda events: any(
            e.event_type == "icon_drawn" and e.icon_name in ["thunder", "thunderstorm"]
            for e in events
        ),
        "icon_snow": lambda events: any(
            e.event_type == "icon_drawn" and e.icon_name in ["snow", "snowfall"]
            for e in events
        ),
        # Umbrella patterns
        "umbrella_drawn": lambda events: any(
            e.event_type == "umbrella_drawn"
            or (e.event_type == "icon_drawn" and e.icon_name == "umbrella")
            for e in events
        ),
        "no_umbrella": lambda events: (
            not any(
                e.event_type == "umbrella_drawn"
                or (e.event_type == "icon_drawn" and e.icon_name == "umbrella")
                for e in events
            )
        ),
        # Probability patterns
        "pop_above_threshold": lambda events: any((e.pop or 0) >= 30 for e in events),
        "pop_high_no_precip": lambda events: any(
            (e.pop or 0) >= 50 and (e.precipitation or 0) == 0 for e in events
        ),
        "pop_low": lambda events: (
            all((e.pop or 100) < 30 for e in events if e.pop is not None)
            or not any(e.pop is not None for e in events)
        ),
        # Precipitation patterns
        "high_precipitation": lambda events: any(
            (e.precipitation or 0) > 10.0 for e in events
        ),
        # Temperature patterns (based on rendered text events with temperature values)
        "temperature_warm": lambda events: any(
            e.event_type == "text_drawn" 
            and any(c in e.icon_name for c in ["22", "23", "24", "25"])
            for e in events
        ),
        "temperature_mild": lambda events: any(
            e.event_type == "text_drawn" 
            and any(c in e.icon_name for c in ["15", "16", "17", "18", "19", "20", "21"])
            for e in events
        ),
        "temperature_freezing": lambda events: any(
            e.event_type == "text_drawn" 
            and any(c in e.icon_name for c in ["-", "0.", "1.", "2."])
            for e in events
        ),
        # Humidity patterns
        "humidity_drawn": lambda events: any(
            e.event_type == "humidity_drawn" for e in events
        ),
        "humidity_high": lambda events: any(
            e.event_type == "humidity_drawn" 
            and any(str(h) in e.icon_name for h in range(80, 101))
            for e in events
        ),
        "humidity_low": lambda events: any(
            e.event_type == "humidity_drawn" 
            and any(str(h) in e.icon_name for h in range(30, 61))
            for e in events
        ),
        # Wind patterns
        "wind_drawn": lambda events: any(
            e.event_type == "wind_drawn" for e in events
        ),
        "wind_calm": lambda events: any(
            e.event_type == "wind_drawn" 
            and any(str(w) in e.icon_name for w in ["3.", "4.", "5.", "6.", "7.", "8.", "9."])
            for e in events
        ),
        "wind_strong": lambda events: any(
            e.event_type == "wind_drawn" 
            and any(str(w) in e.icon_name for w in ["20", "25", "30", "35", "40"])
            for e in events
        ),
    }

    @classmethod
    def validate(
        cls, events: List[RenderEvent], expected_patterns: List[str]
    ) -> ValidationResult:
        """Validate captured events against expected patterns"""
        matched = []
        missing = []
        unexpected = []

        for pattern in expected_patterns:
            if pattern in cls.PATTERN_RULES:
                if cls.PATTERN_RULES[pattern](events):
                    matched.append(pattern)
                else:
                    missing.append(pattern)
            else:
                missing.append(f"{pattern} (unknown)")

        # Check for unexpected umbrella in scenarios that shouldn't have it
        has_umbrella = cls.PATTERN_RULES["umbrella_drawn"](events)
        no_umbrella_expected = "no_umbrella" in expected_patterns

        if has_umbrella and no_umbrella_expected:
            unexpected.append("umbrella_drawn (unexpected)")

        valid = len(missing) == 0 and len(unexpected) == 0

        message = f"Matched: {len(matched)}/{len(expected_patterns)} patterns"
        if missing:
            message += f", Missing: {', '.join(missing)}"
        if unexpected:
            message += f", Unexpected: {', '.join(unexpected)}"

        return ValidationResult(
            scenario="",  # Will be filled by caller
            valid=valid,
            matched_patterns=matched,
            missing_patterns=missing,
            unexpected_events=unexpected,
            captured_events_count=len(events),
            message=message,
            details={
                "events_summary": [
                    {"type": e.event_type, "icon": e.icon_name, "pop": e.pop}
                    for e in events
                ]
            },
        )


# ============================================================================
# Router (FastAPI Application)
# ============================================================================

app = FastAPI(
    title="Weather EPD Mock Server",
    description="Blackbox testing mock server for ESP32 Weather EPD firmware",
    version="2.0.0",
)

# Global state
store = ScenarioStore()


@app.get("/")
async def root():
    """Server info"""
    return {
        "service": "Weather EPD Mock Server",
        "version": "2.0.0",
        "current_scenario": store.current_scenario,
        "available_scenarios": list(store.SCENARIOS.keys()),
    }


# -----------------------------------------------------------------------------
# Open-Meteo API Endpoints
# -----------------------------------------------------------------------------


@app.get("/v1/forecast")
async def get_forecast() -> Dict[str, Any]:
    """
    Returns Open-Meteo format weather forecast for current scenario.
    This endpoint simulates the real Open-Meteo API.
    """
    return store.get_openmeteo_data()


# -----------------------------------------------------------------------------
# Test Control Endpoints
# -----------------------------------------------------------------------------


@app.post("/test/scenario/{name}")
async def set_scenario(
    name: str = Path(
        ...,
        description="Scenario name: sunny, light_rain, heavy_storm, high_precipitation_prob, no_precip_low_prob, snowy",
    ),
) -> Dict[str, Any]:
    """
    Changes the current test scenario.
    Resets the captured events buffer.
    """
    store.set_scenario(name)
    scenario = store.get_scenario()
    return {
        "status": "ok",
        "scenario": name,
        "description": scenario.get("description", ""),
        "expected_patterns": scenario.get("expected_patterns", []),
        "message": f"Scenario set to '{name}'. Event buffer cleared.",
    }


@app.get("/test/scenario")
async def get_current_scenario() -> Dict[str, Any]:
    """Get current scenario info"""
    scenario = store.get_scenario()
    return {
        "current_scenario": store.current_scenario,
        "description": scenario.get("description", ""),
        "expected_patterns": scenario.get("expected_patterns", []),
        "captured_events_count": len(store.captured_events),
    }


# -----------------------------------------------------------------------------
# Event Capture Endpoints (Receives from Firmware Render Mock)
# -----------------------------------------------------------------------------


@app.post("/test/events")
async def receive_events(events: List[RenderEvent]) -> Dict[str, Any]:
    """
    Receives captured render events from the firmware's Render Mock.
    These events represent what the firmware would draw on the display.
    """
    for event in events:
        store.add_event(event)

    return {
        "status": "ok",
        "received": len(events),
        "total_captured": len(store.captured_events),
    }


@app.get("/test/events")
async def get_captured_events() -> List[RenderEvent]:
    """Returns all captured render events for current scenario"""
    return store.captured_events


# -----------------------------------------------------------------------------
# Validation Endpoints (Pattern Matching)
# -----------------------------------------------------------------------------


@app.post("/test/validate")
async def validate_scenario(request: ValidationRequest) -> ValidationResult:
    """
    Validates captured render events against expected patterns for the scenario.
    Uses pattern matching to verify correct icon display logic.
    """
    # Get expected patterns from scenario if not provided
    expected = request.expected_patterns or store.get_expected_patterns(
        request.scenario
    )

    # Run validation
    result = PatternValidator.validate(store.captured_events, expected)
    result.scenario = request.scenario

    store.last_validation = result

    return result


@app.get("/test/validate")
async def get_last_validation() -> Optional[ValidationResult]:
    """Returns the last validation result"""
    if not store.last_validation:
        raise HTTPException(status_code=404, detail="No validation has been run yet")
    return store.last_validation


# -----------------------------------------------------------------------------
# Report Endpoint
# -----------------------------------------------------------------------------


@app.get("/test/report")
async def get_test_report() -> TestReport:
    """
    Returns comprehensive test report including:
    - Current scenario
    - All captured events
    - Last validation result
    - Event statistics
    """
    uptime = datetime.now() - store.start_time

    return TestReport(
        current_scenario=store.current_scenario,
        captured_events=store.captured_events,
        last_validation=store.last_validation,
        total_events_captured=len(store.captured_events),
        server_uptime=str(uptime),
    )


# -----------------------------------------------------------------------------
# Utility Endpoints
# -----------------------------------------------------------------------------


@app.post("/test/reset")
async def reset_server() -> Dict[str, str]:
    """Resets server to initial state"""
    store.set_scenario("sunny")
    store.captured_events = []
    store.last_validation = None
    return {"status": "ok", "message": "Server reset to default state"}


@app.get("/health")
async def health_check() -> Dict[str, Any]:
    """Health check endpoint"""
    return {
        "status": "healthy",
        "scenario": store.current_scenario,
        "captured_events": len(store.captured_events),
        "scenarios_available": len(store.SCENARIOS),
    }


# ============================================================================
# Main Entry Point
# ============================================================================

if __name__ == "__main__":
    print("=" * 60)
    print("Weather EPD Mock Server")
    print("=" * 60)
    print(f"Scenarios: {', '.join(ScenarioStore.SCENARIOS.keys())}")
    print("=" * 60)
    uvicorn.run(app, host="0.0.0.0", port=8080, log_level="info")
