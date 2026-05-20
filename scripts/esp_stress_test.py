#!/usr/bin/env python3
"""True end-to-end ESP latency test with rate-limit awareness."""
import os
os.environ['PYTHONUNBUFFERED'] = '1'

import requests
import time
import statistics
import sys

ESP_HOST = "http://192.168.2.202"
CHAT_URL = f"{ESP_HOST}/chat"
POLL_URL = f"{ESP_HOST}/poll"
CHAT_TIMEOUT = 10
POLL_INTERVAL = 1.0     # 1 second between polls
RESPONSE_TIMEOUT = 120  # generous for LLM generation

# 100 unique prompts with unique markers
BASE_PROMPTS = [
    "你好", "你是谁", "讲个笑话", "笑一个", "哭一个", "开心一下",
    "打个哈欠", "惊讶一下", "思考一下", "无聊",
    "最近怎么样", "你会什么技能", "自我介绍", "现在几点了",
    "设备状态", "内存够用吗", "WiFi信号", "设个提醒",
    "明早叫我起床", "切换人格", "心情不错", "我有点难过",
    "太棒了", "这真令人沮丧", "哈哈太好笑了", "真的吗",
    "有意思", "我不明白", "能再解释吗", "谢谢",
    "好的知道了", "你知道ESP32吗", "PythonvsC", "什么是机器学习",
    "什么是API", "讲个冷笑话", "随便说点什么", "展示能力",
    "你有幽默感吗", "给我惊喜", "你会累吗", "你觉得人类怎么样",
    "你会说外语吗", "我们来聊天", "最近科技进展", "什么是物联网",
    "什么是HTTP", "讲讲WiFi原理", "为什么天空蓝色", "平行世界存在吗",
    "时间是什么", "什么是爱情", "来首诗",
]

PROMPTS = []
for i in range(100):
    marker = f"_R{i:03d}_"
    base = BASE_PROMPTS[i % len(BASE_PROMPTS)]
    PROMPTS.append(f"{marker}{base}")

assert len(set(PROMPTS)) == 100

def p(msg):
    print(msg, flush=True)

def send_chat(msg):
    try:
        body = ("msg=" + msg).encode("utf-8")
        r = requests.post(CHAT_URL, data=body,
                          headers={"Content-Type": "application/x-www-form-urlencoded"},
                          timeout=CHAT_TIMEOUT)
        if r.status_code == 429 or "rate limited" in r.text.lower():
            return "rate_limited"
        return "ok" if r.status_code == 200 else "error"
    except Exception as e:
        return "error"

def wait_for_marker(marker):
    start = time.time()
    while time.time() - start < RESPONSE_TIMEOUT:
        try:
            r = requests.get(POLL_URL, timeout=10)
            if r.status_code == 429:
                time.sleep(5)
                continue
            if r.status_code == 200:
                text = r.content.decode("utf-8", errors="replace")
                if marker in text:
                    return text, time.time() - start
        except Exception as e:
            pass
        time.sleep(POLL_INTERVAL)
    return "", RESPONSE_TIMEOUT

p(f"Target: {CHAT_URL}")
p(f"100 unique prompts, serial end-to-end mode")
p("=" * 60)

results = []
total_start = time.time()

for i, prompt in enumerate(PROMPTS):
    sys.stdout.write(f"[{i+1:3d}/100] \"{prompt[:40]}\" ... "); sys.stdout.flush()

    status = send_chat(prompt)
    if status == "rate_limited":
        p("RATE LIMIT HIT, backing off 60s ...")
        time.sleep(60)
        status = send_chat(prompt)
    if status != "ok":
        p(f"CHAT FAILED ({status})")
        results.append((prompt, None, False, ""))
        continue

    resp, elapsed = wait_for_marker(prompt)

    if elapsed >= RESPONSE_TIMEOUT:
        p(f"TIMEOUT ({elapsed:.0f}s)")
        results.append((prompt, elapsed, False, ""))
    else:
        preview = resp[:50].replace('\n', '\\n')
        p(f"{elapsed:.1f}s | {len(resp)}chars | {preview!r}")
        results.append((prompt, elapsed, True, resp[:80]))

total_time = time.time() - total_start

p("\n" + "=" * 60)
p("SUMMARY")
p("=" * 60)

successful = [(prompt, t) for prompt, t, ok, r in results if ok]
failed = [(i, prompt) for i, (prompt, _, ok, _) in enumerate(results, 1) if not ok]

p(f"  Total rounds:   {len(results)}")
p(f"  Successful:    {len(successful)}")
p(f"  Failed:        {len(failed)}")
p(f"  Error rate:   {len(failed)/len(results)*100:.1f}%")
p(f"  Total time:    {total_time:.1f}s ({total_time/60:.1f}min)")

if successful:
    times = [t for _, t in successful]
    p(f"\n  End-to-end latency (seconds):")
    p(f"    Min:      {min(times):.3f}s")
    p(f"    Max:      {max(times):.3f}s")
    p(f"    Mean:     {statistics.mean(times):.3f}s")
    p(f"    Median:   {statistics.median(times):.3f}s")
    s = sorted(times)
    p95i = min(int(len(s)*0.95), len(s)-1)
    p99i = min(int(len(s)*0.99), len(s)-1)
    p(f"    P95:      {s[p95i]:.3f}s")
    p(f"    P99:      {s[p99i]:.3f}s")

if failed:
    p(f"\n  Failed ({len(failed)}):")
    for idx, prompt in failed[:10]:
        p(f"    [{idx:3d}] \"{prompt}\"")
