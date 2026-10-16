//! Fixed-capacity, audio-owner scheduling. The inter-thread queue is separate.
use crate::{Command, Error, MAX_TRACKS};

pub const SCHEDULE_CAPACITY: usize = 256;

#[derive(Clone, Copy, Debug)]
pub struct ScheduledCommand {
    pub frame: u64,
    pub track: usize,
    pub command: Command,
}

#[derive(Clone, Copy, Default, Debug, PartialEq, Eq)]
pub struct ScheduleStatus {
    pub pending: usize,
    pub applied: u64,
    pub rejected: u64,
    pub last_error: Option<Error>,
}

pub(crate) struct Schedule {
    entries: [Option<ScheduledCommand>; SCHEDULE_CAPACITY],
    head: usize,
    length: usize,
    pub(crate) status: ScheduleStatus,
}

impl Schedule {
    pub(crate) fn new() -> Self {
        Self {
            entries: [None; SCHEDULE_CAPACITY],
            head: 0,
            length: 0,
            status: ScheduleStatus::default(),
        }
    }

    pub(crate) fn push(&mut self, event: ScheduledCommand, clock: u64) -> Result<(), Error> {
        if event.track >= MAX_TRACKS {
            return Err(Error::InvalidTrack);
        }
        if event.frame < clock {
            return Err(Error::ScheduleInPast);
        }
        if self.length == SCHEDULE_CAPACITY {
            return Err(Error::ScheduleFull);
        }
        if self.length > 0 {
            let tail = (self.head + self.length - 1) % SCHEDULE_CAPACITY;
            if event.frame < self.entries[tail].unwrap().frame {
                return Err(Error::ScheduleOrder);
            }
        }
        self.entries[(self.head + self.length) % SCHEDULE_CAPACITY] = Some(event);
        self.length += 1;
        self.status.pending = self.length;
        Ok(())
    }

    pub(crate) fn pop_due(&mut self, frame: u64) -> Option<ScheduledCommand> {
        if self.length == 0 || self.entries[self.head]?.frame > frame {
            return None;
        }
        let event = self.entries[self.head].take();
        self.head = (self.head + 1) % SCHEDULE_CAPACITY;
        self.length -= 1;
        self.status.pending = self.length;
        event
    }

    pub(crate) fn result(&mut self, result: Result<(), Error>) {
        match result {
            Ok(()) => self.status.applied = self.status.applied.saturating_add(1),
            Err(error) => {
                self.status.rejected = self.status.rejected.saturating_add(1);
                self.status.last_error = Some(error);
            }
        }
    }

    pub(crate) fn cancel(&mut self, track: usize) {
        let old_length = self.length;
        let mut kept = 0;
        for index in 0..old_length {
            let event = self.entries[(self.head + index) % SCHEDULE_CAPACITY]
                .take()
                .unwrap();
            if event.track != track {
                self.entries[(self.head + kept) % SCHEDULE_CAPACITY] = Some(event);
                kept += 1;
            }
        }
        self.length = kept;
        self.status.pending = kept;
    }
}
