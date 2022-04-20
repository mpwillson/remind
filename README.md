# remind

A C command line program to issue reminders for actions and events.

## Synopsis

    remind  [-a] [-c colour_pairs] [-D n[,n] ...] [-d date] [-e]
            [-f filename] [-h] [-i] [-L] [-l] [-m n[,n] ...] [-P pointer]
            [-p] [-q] [-r repeat] [-s] [-t timeout]
            [-u urgency] [-w warning] [-X n[,n] ...]
            [message]

See remind(1) man page for more.

## ChangeLog

### 1.3.2

* Fix incorrect calculation of time to event when crossing DST
  boundary.

### 1.3.1

* Correctly warn about periodic actions in the following year.
* Prevent modification or deletion of freed actions.
* Improve remind file error handling.

### 1.3

* Honour urgency setting when reporting on periodic actions.
* If REMIND_TIME is set, current time of day is retained for new date.
* Ensure periodic action is default when first word of message is a date.

### 1.2

* Make action timeout deletion actually obey timeout value.
* Default action datetime is now set at end of current day.
* Cleanup of Makefile release target.

### 1.1

* Add support for repeating periodic action on the Nth weekday of
  every Mth week (e.g. every second Monday).
* REMIND_TIME environment variable to set effective time of remind execution.
* Fix off-by-one error when warning for periodic actions.
* Fix timeout action deletion.

### 1.0

* First release.
