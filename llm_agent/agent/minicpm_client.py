"""调用本地 MiniCPM-o OpenAI 兼容服务。"""
from __future__ import annotations
import os
import tempfile
from pathlib import Path
from openai import OpenAI


class MiniCpmClient:
    def __init__(self) -> None:
        self.client = OpenAI(base_url=os.getenv("MINICPM_BASE_URL", "http://127.0.0.1:8000/v1"), api_key=os.getenv("MINICPM_API_KEY", "EMPTY"))
        self.model = os.getenv("MINICPM_MODEL", "minicpm-o-4.5-awq")

    def respond(self, event: dict) -> str:
        content = [{"type": "text", "text": "你是小车语音助手。结合用户语音和当前画面，用简洁中文回答。"}]
        image = event["perception"].get("image_data_url")
        if image:
            content.append({"type": "image_url", "image_url": {"url": image}})
        audio_path: Path | None = None
        try:
            if event.get("speech_wav"):
                with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as file:
                    file.write(event["speech_wav"])
                    audio_path = Path(file.name)
                # OpenAI 客户端要求 URL 具有协议；模型服务与 Agent 在同一 WSL，
                # 因此使用 file:// 让 vLLM 读取本机临时 WAV。
                content.append({"type": "audio_url", "audio_url": {"url": audio_path.as_uri()}})
            result = self.client.chat.completions.create(model=self.model, messages=[{"role": "user", "content": content}], max_tokens=256, temperature=0.2)
            return result.choices[0].message.content or "（模型未返回文本）"
        finally:
            if audio_path:
                audio_path.unlink(missing_ok=True)
