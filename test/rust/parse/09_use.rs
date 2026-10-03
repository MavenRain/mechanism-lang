//! Use trees and modules.

use std::collections::{BTreeMap, HashMap};
use std::fmt;
use std::io::*;
use std::rc::Rc as Shared;

pub(crate) mod inner {
    //! An inline module.

    pub fn id(x: i64) -> i64 {
        x
    }
}

pub mod empty {}
