"""
settings.py - reads the AI key and the email (SMTP) login.

Secrets live in config.json next to this file (copy config.example.json and
fill it in). config.json is listed in .gitignore, so it is never committed,
and nothing in it is ever sent to the browser.

Environment variables override the file, which is handy on a lab computer:
  SRP_AI_PROVIDER, SRP_AI_API_KEY, SRP_AI_MODEL, SRP_AI_FALLBACK_MODEL, SRP_AI_BASE_URL,
  SRP_SMTP_HOST, SRP_SMTP_PORT, SRP_SMTP_USER, SRP_SMTP_PASSWORD
"""

import json
import os

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(PROJECT_DIR, "config.json")

PROVIDERS = ("gemini", "openai", "anthropic")
DEFAULT_MODELS = {
    # A Google alias that always points at the current Flash model.
    "gemini": "gemini-flash-latest",
    "anthropic": "claude-haiku-5-5",
    # OpenAI model names change often; set "model" in config.json yourself.
    "openai": "",
}
# Tried when the main model stays busy (HTTP 503 / 429) after 3 tries.
DEFAULT_FALLBACKS = {"gemini": "gemini-flash-lite-latest", "anthropic": "", "openai": ""}
PLACEHOLDER_PREFIX = "PASTE"


def _read_file():
    if not os.path.exists(CONFIG_PATH):
        return {}
    try:
        with open(CONFIG_PATH, encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError) as e:
        print(f"config.json could not be read ({e}). Check that it is valid JSON.")
        return {}


def load():
    data = _read_file()
    ai = data.get("ai", {}) if isinstance(data.get("ai"), dict) else {}
    email = data.get("email", {}) if isinstance(data.get("email"), dict) else {}
    env = os.environ.get

    provider = (env("SRP_AI_PROVIDER") or ai.get("provider") or "gemini").strip().lower()
    api_key = (env("SRP_AI_API_KEY") or ai.get("api_key") or "").strip()
    if api_key.startswith(PLACEHOLDER_PREFIX):
        api_key = ""
    model = (env("SRP_AI_MODEL") or ai.get("model") or DEFAULT_MODELS.get(provider, "")).strip()

    smtp_user = (env("SRP_SMTP_USER") or email.get("username") or "").strip()
    smtp_password = (env("SRP_SMTP_PASSWORD") or email.get("password") or "").strip()
    if smtp_password.startswith(PLACEHOLDER_PREFIX):
        smtp_password = ""
    try:
        port = int(env("SRP_SMTP_PORT") or email.get("smtp_port") or 587)
    except ValueError:
        port = 587

    return {
        "ai": {
            "provider": provider,
            "api_key": api_key,
            "model": model,
            "base_url": (env("SRP_AI_BASE_URL") or ai.get("base_url") or "").strip(),
            # Gemini only: "", "minimal", "low", "medium" or "high". "" = the model's default.
            "thinking": (env("SRP_AI_THINKING") or ai.get("thinking") or "").strip().lower(),
            "fallback_model": (env("SRP_AI_FALLBACK_MODEL") or ai.get("fallback_model")
                               or DEFAULT_FALLBACKS.get(provider, "")).strip(),
        },
        "email": {
            "smtp_host": (env("SRP_SMTP_HOST") or email.get("smtp_host") or "smtp.gmail.com").strip(),
            "smtp_port": port,
            "username": smtp_user,
            "password": smtp_password,
            "from_name": (email.get("from_name") or "Smart Revision").strip(),
        },
    }


def ai_problem(cfg):
    """Returns why the AI can't be used, or "" if it is ready."""
    ai = cfg["ai"]
    if ai["provider"] not in PROVIDERS:
        return f"Unknown AI provider \"{ai['provider']}\". Use gemini, openai or anthropic in config.json."
    if not ai["api_key"]:
        return "No AI API key is set. Copy config.example.json to config.json and paste your key."
    if not ai["model"]:
        return "Set the model name (ai.model) in config.json."
    return ""


def email_ready(cfg):
    e = cfg["email"]
    return bool(e["smtp_host"] and e["username"] and e["password"])
