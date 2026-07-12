from __future__ import annotations

import asyncio

import pytest

from recon.scheduler import Scheduler, Job, run_scheduler


async def sample_job(**kwargs):
    pass


def test_scheduler_add_job():
    scheduler = Scheduler()
    scheduler.add_job("test_job", sample_job, 60.0)
    jobs = scheduler.list_jobs()
    assert len(jobs) == 1
    assert jobs[0]["name"] == "test_job"
    assert jobs[0]["interval"] == 60.0


def test_scheduler_list_jobs():
    scheduler = Scheduler()
    scheduler.add_job("job1", sample_job, 30.0)
    scheduler.add_job("job2", sample_job, 60.0)
    jobs = scheduler.list_jobs()
    assert len(jobs) == 2


def test_scheduler_initial_state():
    scheduler = Scheduler()
    scheduler.add_job("test", sample_job, 60.0)
    jobs = scheduler.list_jobs()
    assert jobs[0]["last_run"] is None
    assert jobs[0]["running"] is False
    assert jobs[0]["error"] is None


@pytest.mark.asyncio
async def test_job_run():
    run_count = 0

    async def counting_job(**kwargs):
        nonlocal run_count
        run_count += 1

    job = Job("counter", counting_job, 0.1)
    await job.run()
    assert run_count == 1
    assert job.last_run is not None
    assert job.running is False


@pytest.mark.asyncio
async def test_job_error_handling():
    async def failing_job(**kwargs):
        raise ValueError("test error")

    job = Job("failing", failing_job, 0.1)
    await job.run()
    assert job.error == "test error"
    assert job.running is False


@pytest.mark.asyncio
async def test_job_skip_if_already_running():
    run_count = 0

    async def slow_job(**kwargs):
        nonlocal run_count
        run_count += 1
        await asyncio.sleep(0.5)

    job = Job("slow", slow_job, 0.1)
    # Start first run
    task1 = asyncio.create_task(job.run())
    await asyncio.sleep(0.05)
    # Try to run again while first is running
    await job.run()
    assert run_count == 1  # Should skip the second run
    await task1


@pytest.mark.asyncio
async def test_scheduler_stop():
    scheduler = Scheduler()
    scheduler.add_job("test", sample_job, 0.01)

    async def run_and_stop():
        task = asyncio.create_task(scheduler.start())
        await asyncio.sleep(0.1)
        scheduler.stop()
        await task

    await run_and_stop()
    assert not scheduler._running


def test_job_kwargs():
    received = {}

    async def capturing_job(**kwargs):
        received.update(kwargs)

    job = Job("capturing", capturing_job, 60.0, key="value", number=42)
    asyncio.get_event_loop().run_until_complete(job.run())
    assert received["key"] == "value"
    assert received["number"] == 42
