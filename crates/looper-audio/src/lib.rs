//! Native audio adapters. Device setup runs on the control thread.
#![forbid(unsafe_code)]

pub mod capture;
pub mod device;
pub mod duplex;
pub mod input;
pub mod realtime;
