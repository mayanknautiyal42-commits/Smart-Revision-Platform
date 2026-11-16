"""
mailer.py - sends the password-reset OTP by email (SMTP, standard library).

For Gmail: turn on 2-step verification, create an "App password", and put
that 16-character password in config.json (never your normal password).
"""

import smtplib
import ssl
from email.message import EmailMessage


class MailError(Exception):
    pass


def send_otp(cfg, to_address, name, code, minutes, role):
    e = cfg["email"]
    msg = EmailMessage()
    msg["Subject"] = f"Your Smart Revision OTP: {code}"
    msg["From"] = f"{e['from_name']} <{e['username']}>"
    msg["To"] = to_address
    account = "admin" if role == "admin" else "student"
    msg.set_content(
        f"Hi {name},\n\n"
        f"Use this code to reset the password of your Smart Revision {account} account:\n\n"
        f"    {code}\n\n"
        f"It works for {minutes} minutes and only once. If you didn't ask for it, ignore this email; "
        f"your password stays the same.\n"
    )
    context = ssl.create_default_context()
    try:
        if e["smtp_port"] == 465:
            with smtplib.SMTP_SSL(e["smtp_host"], 465, context=context, timeout=20) as server:
                server.login(e["username"], e["password"])
                server.send_message(msg)
        else:
            with smtplib.SMTP(e["smtp_host"], e["smtp_port"], timeout=20) as server:
                server.starttls(context=context)
                server.login(e["username"], e["password"])
                server.send_message(msg)
    except smtplib.SMTPAuthenticationError:
        raise MailError("The email login in config.json was rejected (for Gmail, use an App password).")
    except (smtplib.SMTPException, OSError) as ex:
        print(f"Email failed: {ex}")
        raise MailError("The OTP email could not be sent. Check the email settings and the internet connection.")
