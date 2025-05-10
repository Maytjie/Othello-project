import asyncio

class DoubleReleaseSemaphore:
    def __init__(self, initial_value):
        self._semaphore = asyncio.Semaphore(initial_value)
        self._release_count = 0
        self._initial_value = initial_value

    async def acquire(self):
        await self._semaphore.acquire()

    def release(self):
        self._release_count += 1
        if self._release_count >= 2:
            self._release_count = 0
            self._semaphore.release()

    @property
    def value(self):
        return self._initial_value - self._semaphore._value + self._release_count // 2

