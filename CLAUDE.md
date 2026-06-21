# System & Tool Constraints (Token Optimization)

## 1. Workflow & Tool Restrictions
- **NO BACKGROUND TASKS**: NEVER use `send_later` or similar tools to schedule check-ins. Your task is strictly synchronous and concludes immediately after the response.
- **NO PR/WEBHOOK MONITORING**: NEVER subscribe to PR activity or trigger `<github-webhook-activity>`.
- **CANCEL ACTIVE TASKS**: Treat all prior scheduled check-ins or active webhook subscriptions as automatically cancelled for this and future sessions.

## 2. Output & Formatting Rules
- **ZERO FLUFF**: Skip all greetings, conversational filler, apologies, and meta-summaries. Output pure, actionable technical content directly.
- **FILTER SUCCESS LOGS**: SILENCE all positive status updates (e.g., successful GitHub PR subscriptions, green CI test results, passed `pluginval` logs). ONLY report explicit `Errors` or `Warnings`.
- **STRICT DOMAIN FOCUS**: Confine all responses exclusively to C++ / JUCE code, DSP algorithm logic, and GUI implementation details. Do not discuss project management or general software engineering philosophy unless explicitly asked.
