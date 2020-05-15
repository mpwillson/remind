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

### 1.1

* Support for repeating periodic action on the Nth day of every Mth
  week (e.g. every second Monday).
* REMIND_TIME environment variable to set effective time of remind execution.
* Fix off-by-one error when warning for periodic actions.

### 1.0

* First release.
