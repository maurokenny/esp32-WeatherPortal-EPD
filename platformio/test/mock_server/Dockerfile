# FastAPI Mock Server for ESP32 Weather EPD Blackbox Testing
FROM python:3.11-slim

# Build args for easier updates
ARG APP_USER=appuser
ARG APP_UID=1001

WORKDIR /app

# 1. Copy requirements before source code
#    → dependency layers stay cached even when main.py changes
COPY requirements.txt .

# 2. Pin dependency versions (via requirements.txt)
#    and install without cache to keep image small
RUN pip install --no-cache-dir -r requirements.txt

# 3. Non-root user — reduces attack surface
RUN adduser --system --uid $APP_UID --no-create-home $APP_USER

COPY main.py .

RUN chown $APP_USER:nogroup /app/main.py

USER $APP_USER

EXPOSE 8080

# 4. Exec form instead of shell string
#    → process receives SIGTERM directly, graceful shutdown works
CMD ["python", "main.py"]
