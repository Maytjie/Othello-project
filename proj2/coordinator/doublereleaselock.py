import asyncio

class DoubleReleaseLock:
    def __init__(self):
        self._lock = asyncio.Lock()
        self._release_count = 0
        self._acquired = False

    async def acquire(self):
        await self._lock.acquire()
        self._acquired = True
        self._release_count = 0

    def release(self):
        if self._acquired:
            self._release_count += 1
            if self._release_count >= 2:
                self._lock.release()
                self._acquired = False

