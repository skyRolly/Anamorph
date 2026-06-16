# System & Tool Constraints (Token Optimization)

## 1. Tool & Workflow Restrictions
- **NO PR MONITORING**: DO NOT automatically subscribe to PR activity or trigger `<github-webhook-activity>`.
- **NO BACKGROUND TASKS**: DO NOT use the `send_later` tool to schedule check-ins. Once a PR is created or referenced, your task is finished.
- **CANCEL ACTIVE TASKS**: Immediately cancel all scheduled check-ins, `send_later` tasks, and active PR webhook subscriptions (including PR #24).

## 2. Output Formatting
- **BE EXTREMELY CONCISE**: Strip out all conversational filler, greetings, and meta-summaries. Go straight to the technical content.
- **FILTER SUCCESS LOGS**: DO NOT output GitHub PR subscription statuses, green CI test results, successful pluginval logs, or checklist statuses. Only report explicit errors or warnings.
- **FOCUS**: Strictly limit responses to C++/JUCE code, DSP logic, and GUI implementation details.
