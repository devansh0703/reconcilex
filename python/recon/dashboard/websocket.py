from __future__ import annotations

import asyncio
import json
import logging
from typing import Any

logger = logging.getLogger(__name__)

try:
    import websockets
    import websockets.server
except ImportError:
    websockets = None  # type: ignore


class ExceptionBroadcaster:
    """WebSocket broadcaster for live exception updates."""

    def __init__(self) -> None:
        self._clients: set[Any] = set()
        self._queue: asyncio.Queue[dict[str, Any]] | None = None

    async def handler(self, websocket: Any) -> None:
        self._clients.add(websocket)
        logger.info("WebSocket client connected (%d total)", len(self._clients))
        try:
            async for message in websocket:
                pass  # Client messages ignored for now
        except Exception:
            pass
        finally:
            self._clients.discard(websocket)
            logger.info("WebSocket client disconnected (%d total)", len(self._clients))

    async def broadcast(self, event: dict[str, Any]) -> None:
        if not self._clients:
            return
        msg = json.dumps(event)
        dead = set()
        for ws in self._clients:
            try:
                await ws.send(msg)
            except Exception:
                dead.add(ws)
        self._clients -= dead

    async def broadcast_exception(self, exception: dict[str, Any]) -> None:
        await self.broadcast({"type": "exception", "data": exception})

    async def broadcast_status_update(self, exception_id: int, new_state: str) -> None:
        await self.broadcast({
            "type": "status_update",
            "exception_id": exception_id,
            "new_state": new_state,
        })


broadcaster = ExceptionBroadcaster()


async def start_websocket_server(host: str = "0.0.0.0", port: int = 8765) -> None:
    if websockets is None:
        logger.warning("websockets not installed, skipping server")
        return

    async with websockets.serve(broadcaster.handler, host, port):
        logger.info("WebSocket server started on ws://%s:%d", host, port)
        await asyncio.Future()  # run forever
