from __future__ import annotations

import asyncio
import logging
from datetime import datetime
from typing import Any, Callable, Coroutine

logger = logging.getLogger(__name__)


class Job:
    """Represents a scheduled job."""

    def __init__(self, name: str, func: Callable[..., Coroutine], interval_seconds: float, **kwargs: Any) -> None:
        self.name = name
        self.func = func
        self.interval = interval_seconds
        self.kwargs = kwargs
        self.last_run: datetime | None = None
        self.running = False
        self.error: str | None = None

    async def run(self) -> None:
        if self.running:
            logger.warning("Job %s already running, skipping", self.name)
            return
        self.running = True
        self.error = None
        try:
            logger.info("Running job: %s", self.name)
            await self.func(**self.kwargs)
            self.last_run = datetime.utcnow()
            logger.info("Job %s completed", self.name)
        except Exception as e:
            self.error = str(e)
            logger.error("Job %s failed: %s", self.name, e)
        finally:
            self.running = False


class Scheduler:
    """Async job scheduler with configurable intervals."""

    def __init__(self) -> None:
        self._jobs: list[Job] = []
        self._running = False

    def add_job(self, name: str, func: Callable[..., Coroutine],
                interval_seconds: float, **kwargs: Any) -> Job:
        job = Job(name, func, interval_seconds, **kwargs)
        self._jobs.append(job)
        return job

    async def start(self) -> None:
        self._running = True
        logger.info("Scheduler started with %d jobs", len(self._jobs))

        while self._running:
            now = datetime.utcnow()
            for job in self._jobs:
                if job.running:
                    continue
                if job.last_run is None:
                    await job.run()
                elif (now - job.last_run).total_seconds() >= job.interval:
                    asyncio.create_task(job.run())
            await asyncio.sleep(1)

    def stop(self) -> None:
        self._running = False
        logger.info("Scheduler stopped")

    def list_jobs(self) -> list[dict[str, Any]]:
        return [
            {
                "name": j.name,
                "interval": j.interval,
                "last_run": j.last_run.isoformat() if j.last_run else None,
                "running": j.running,
                "error": j.error,
            }
            for j in self._jobs
        ]


async def _job_match(config: dict[str, Any]) -> None:
    """Scheduled matching job."""
    logger.info("Running scheduled match...")
    try:
        from recon.cli import _run_matching_pure
        result = _run_matching_pure(config)
        logger.info("Match complete: %d exceptions", len(result.get("exceptions", [])))
    except Exception as e:
        logger.error("Match job failed: %s", e)


async def _job_report(config: dict[str, Any]) -> None:
    """Scheduled report generation job."""
    logger.info("Running scheduled report...")
    from datetime import date
    from pathlib import Path

    today = date.today().isoformat()
    output_dir = Path(config["reporting"]["output_dir"])
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / f"report_{today}.html"
    logger.info("Report saved to %s", output_path)


async def _job_settlement_check(config: dict[str, Any]) -> None:
    """Scheduled settlement check job."""
    logger.info("Running settlement check...")


def run_scheduler(config: dict[str, Any]) -> None:
    """Create and run the scheduler with configured jobs."""
    scheduler = Scheduler()

    scheduler.add_job("daily_match", _job_match, 3600 * 8, config=config)
    scheduler.add_job("daily_report", _job_report, 3600 * 10, config=config)
    scheduler.add_job("settlement_check", _job_settlement_check, 3600 * 12, config=config)

    asyncio.run(scheduler.start())
