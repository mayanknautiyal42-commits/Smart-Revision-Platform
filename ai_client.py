"""
ai_client.py - sends one prompt to the chosen AI provider and returns JSON.

Only the Python standard library is used (urllib), so nothing has to be
installed with pip. Supported providers: gemini, openai, anthropic.
"openai" also works with any OpenAI-compatible server if base_url is set.
"""

import concurrent.futures
import json
import re
import time
from time import perf_counter   # separate name: tests replace ai_client.time with a fake
import urllib.error
import urllib.request

RETRY_WAITS = [2, 5]          # seconds to wait before the 2nd and 3rd try
RETRY_STATUS = (429, 500, 502, 503, 504)


class AIError(Exception):
    """A problem talking to the AI, with a message that can be shown to users."""

    def __init__(self, message, status=0):
        super().__init__(message)
        self.status = status      # the HTTP status code, when there was one


class BusyError(AIError):
    """The provider is overloaded or rate-limited: worth trying again later."""

    def __init__(self, message, wait=0):
        super().__init__(message, 503)
        self.wait = wait


def _post(url, headers, body, timeout):
    data = json.dumps(body).encode("utf-8")
    request = urllib.request.Request(url, data=data, method="POST",
                                     headers={"Content-Type": "application/json", **headers})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        detail = ""
        try:
            detail = e.read().decode("utf-8", "replace")[:300]
        except Exception:
            pass
        print(f"AI request failed: HTTP {e.code} {detail}")
        if e.code in (401, 403):
            raise AIError("The AI provider rejected the API key. Check ai.api_key in config.json.")
        if e.code == 404:
            raise AIError("The AI model was not found. Check ai.model in config.json.")
        if e.code in RETRY_STATUS:
            try:
                wait = min(int(e.headers.get("Retry-After", "0")), 20)
            except (TypeError, ValueError):
                wait = 0
            if e.code == 429:
                raise BusyError("The AI's free limit was reached for now. Wait a minute and try again.", wait)
            raise BusyError("The AI servers are busy right now (this is on the provider's side). "
                            "Wait a minute and try again.", wait)
        raise AIError(f"The AI provider returned an error (HTTP {e.code}). Try again.", e.code)
    except (urllib.error.URLError, TimeoutError, OSError) as e:
        print(f"AI request failed: {e}")
        if "timed out" in str(e).lower():
            raise BusyError("The AI took too long to answer. Try again.")
        raise AIError("Could not reach the AI provider. Check the internet connection.")
    except ValueError:
        raise AIError("The AI provider sent a reply that was not JSON.")


THINKING_UNSUPPORTED = set()   # models that refused the thinking setting (HTTP 400)


def _gemini(ai, system, prompt, max_tokens, timeout):
    url = f"https://generativelanguage.googleapis.com/v1beta/models/{ai['model']}:generateContent"
    body = {
        "systemInstruction": {"parts": [{"text": system}]},
        "contents": [{"role": "user", "parts": [{"text": prompt}]}],
        "generationConfig": {"responseMimeType": "application/json", "maxOutputTokens": max_tokens},
    }
    # Less "thinking" = faster answers, but more risk of a wrong answer key.
    level = ai.get("thinking", "")
    if level and ai["model"] not in THINKING_UNSUPPORTED:
        body["generationConfig"]["thinkingConfig"] = {"thinkingLevel": level}
    try:
        reply = _post(url, {"x-goog-api-key": ai["api_key"]}, body, timeout)
    except AIError as e:
        if e.status != 400 or "thinkingConfig" not in body["generationConfig"]:
            raise
        print(f"Model {ai['model']} doesn't accept thinking level '{level}'; using its default.")
        THINKING_UNSUPPORTED.add(ai["model"])
        del body["generationConfig"]["thinkingConfig"]
        reply = _post(url, {"x-goog-api-key": ai["api_key"]}, body, timeout)
    try:
        parts = reply["candidates"][0]["content"]["parts"]
        return "".join(p.get("text", "") for p in parts if not p.get("thought"))
    except (KeyError, IndexError, TypeError):
        print(f"Unexpected Gemini reply: {str(reply)[:300]}")
        raise AIError("The AI gave an empty answer (it may have been blocked). Try again.")


def _openai(ai, system, prompt, max_tokens, timeout):
    base = (ai.get("base_url") or "https://api.openai.com/v1").rstrip("/")
    body = {
        "model": ai["model"],
        "messages": [{"role": "system", "content": system}, {"role": "user", "content": prompt}],
        "response_format": {"type": "json_object"},
    }
    reply = _post(base + "/chat/completions", {"Authorization": "Bearer " + ai["api_key"]}, body, timeout)
    try:
        return reply["choices"][0]["message"]["content"] or ""
    except (KeyError, IndexError, TypeError):
        raise AIError("The AI gave an empty answer. Try again.")


def _anthropic(ai, system, prompt, max_tokens, timeout):
    body = {
        "model": ai["model"],
        "max_tokens": max_tokens,
        "system": system,
        "messages": [{"role": "user", "content": prompt}],
    }
    headers = {"x-api-key": ai["api_key"], "anthropic-version": "2023-06-01"}
    reply = _post("https://api.anthropic.com/v1/messages", headers, body, timeout)
    try:
        return "".join(block.get("text", "") for block in reply["content"] if block.get("type") == "text")
    except (KeyError, TypeError):
        raise AIError("The AI gave an empty answer. Try again.")


CALLERS = {"gemini": _gemini, "openai": _openai, "anthropic": _anthropic}


def extract_json(text):
    """Models sometimes wrap JSON in ``` fences or add a sentence; keep only the object."""
    text = text.strip()
    text = re.sub(r"^```(?:json)?\s*|\s*```$", "", text)
    try:
        return json.loads(text)
    except ValueError:
        start, end = text.find("{"), text.rfind("}")
        if start != -1 and end > start:
            try:
                return json.loads(text[start:end + 1])
            except ValueError:
                pass
    raise AIError("The AI reply could not be read as JSON. Try again.")


def _ask_with_retries(caller, ai, system, prompt, max_tokens, timeout):
    """Up to 3 tries when the provider is busy (HTTP 429/5xx), waiting a little longer each time."""
    for attempt in range(len(RETRY_WAITS) + 1):
        started = perf_counter()
        try:
            text = caller(ai, system, prompt, max_tokens, timeout)
            # Timing log: shows whether the delay is the AI itself or retries/waits.
            print(f"AI reply from {ai['model']} in {perf_counter() - started:.1f} s "
                  f"(try {attempt + 1}, {len(text)} chars)")
            return text
        except BusyError as e:
            if attempt == len(RETRY_WAITS):
                raise
            wait = max(RETRY_WAITS[attempt], e.wait)
            print(f"AI busy ({ai['model']}), trying again in {wait} s...")
            time.sleep(wait)


def _hedged(caller, ai, system, prompt, max_tokens, timeout, hedge_after):
    """Speed trick ("hedged request"): the same request sometimes takes 1 s and sometimes 15 s
    on the provider's side. If no reply has come after hedge_after seconds, send the same
    request once more and use whichever reply arrives first. Costs an extra request only
    when the first one is slow."""
    pool = concurrent.futures.ThreadPoolExecutor(max_workers=2)
    try:
        first = pool.submit(_ask_with_retries, caller, ai, system, prompt, max_tokens, timeout)
        try:
            return first.result(timeout=hedge_after)
        except concurrent.futures.TimeoutError:
            pass
        print(f"AI slow (> {hedge_after} s), sending a second copy of the request...")
        second = pool.submit(_ask_with_retries, caller, ai, system, prompt, max_tokens, timeout)
        pending = {first, second}
        error = None
        while pending:
            done, pending = concurrent.futures.wait(pending, return_when=concurrent.futures.FIRST_COMPLETED)
            for future in done:
                try:
                    return future.result()
                except AIError as e:
                    error = error or e
        raise error
    finally:
        pool.shutdown(wait=False)      # the slower copy finishes on its own; its reply is ignored


def complete_json(cfg, system, prompt, max_tokens=8000, timeout=150, hedge_after=None):
    ai = cfg["ai"]
    caller = CALLERS.get(ai["provider"])
    if caller is None:
        raise AIError("Unknown AI provider in config.json.")
    try:
        if hedge_after:
            text = _hedged(caller, ai, system, prompt, max_tokens, timeout, hedge_after)
        else:
            text = _ask_with_retries(caller, ai, system, prompt, max_tokens, timeout)
    except BusyError as busy:
        # Still busy after 3 tries: use the backup model once, if there is one.
        backup = ai.get("fallback_model", "")
        if not backup or backup == ai["model"]:
            raise
        print(f"Switching to the backup model {backup}.")
        try:
            text = _ask_with_retries(caller, dict(ai, model=backup), system, prompt, max_tokens, timeout)
        except BusyError:
            raise busy
        except AIError as e:
            print(f"Backup model failed: {e}")
            raise busy
    result = extract_json(text)
    if not isinstance(result, dict):
        raise AIError("The AI reply had the wrong shape. Try again.")
    return result
