all: clean
	gcc -Wall -o cook cook.c
	gcc -Wall -o waiter waiter.c
	gcc -Wall -o customer customer.c
	gcc -Wall -o gencustomers gencustomers.c
	./gencustomers > customers.txt
	# you can simply "make run" which gives you the 3 transcript files

run: all
	./cook > transcript1_cook.txt &
	./waiter > transcript2_waiter.txt &
	./customer > transcript3_customer.txt

clean:
	rm -f cook waiter customer gencustomers
	ipcs -s | grep 0x00001234 | awk '{print "ipcrm -s " $$2 }' | sh
	ipcs -s | grep 0x00005678 | awk '{print "ipcrm -s " $$2 }' | sh
	ipcs -s | grep 0x00005679 | awk '{print "ipcrm -s " $$2 }' | sh
	ipcs -s | grep 0x00005680 | awk '{print "ipcrm -s " $$2 }' | sh
	ipcs -s | grep 0x00005681 | awk '{print "ipcrm -s " $$2 }' | sh
	# run "make cleanall" to remove customers.txt and transcript files

cleanall:
	rm -f cook waiter customer gencustomers transcript1_cook.txt transcript2_waiter.txt transcript3_customer.txt
