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


