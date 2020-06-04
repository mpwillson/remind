#remind: simpel test script
export REMIND_FILE=./remind.db
export REMIND_TIME=01/01/2030
echo Initialisation
./remind -iq
echo Define periodic repeats
./remind -w 25 -r n1,1 First Monday in the month
./remind -w 25 -r n2,2 Second Tuesday in the month
./remind -w 25 -r n3,3 Third Wednesday in the month
./remind -w 25 -r n4,4 Fourth Thursday in the month
./remind -w 25 -r n5,5 Fifth Friday in the month
./remind -w 25 -r n6,6 Last Saturday in the month
./remind -w 8 -r w2 Every Tuesday
./remind -u 1 4/1 4th January periodic
echo Display
./remind
echo Define standard action
./remind -s 02/01 Delayed standard action
echo Display
./remind
echo List actions
./remind -l
echo List actions with header
./remind -L
echo Delete delayed action
./remind -D 9
echo Delete free record
./remind -D 9
echo Modify free record
./remind -m 9
echo Define standard action with timeout
./remind -t 1 Standard action for timeout delete
echo Display standard actions
./remind -s
echo Advance time to trigger timeout
REMIND_TIME=02/01/2030
./remind -s
echo List actions with header
./remind -L
echo Re-initialise
yes|./remind -i -c 1,2 3,4 5,6 7,8
echo Urgencies
./remind -u 1 Urgency 1
./remind -u 2 Urgency 2
./remind -u 3 Urgency 3
./remind -u 4 Urgency 4
./remind -h
./remind -u 4
./remind -u 3
./remind -u 2
./remind -u 1
./remind -m 1 -u 0 Urgency 0
./remind
./remind -u 0
echo Day of week repeats
./remind -r w2,1 -w 7 Every Tuesday
./remind -r w1,2 -w 14 Every second Monday
./remind -r w5,2 -w 14 Every second Friday
./remind -p
echo Report on 9th Jan
REMIND_TIME=09/01/2030
./remind -p
echo Month repeats
./remind -iq
./remind 10/1 Yearly, 10th January
./remind -r m 10/1 10th of every month
./remind
REMIND_TIME=09/02/2030
./remind
echo Export
./remind A standard action
./remind -s 11/2 A delayed standard action
./remind -r w2,1 -w 7 Every Tuesday
./remind -L
./remind -e >/tmp/remind$$.sh
sh /tmp/remind$$.sh
rm /tmp/remind$$.sh
echo After re-creation
./remind -L
./remind
