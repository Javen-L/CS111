# NAME: Jianzhi Liu, Yinsheng Zhou
# EMAIL: ljzprivate@yahoo.com, jacobzhou@g.ucla.edu
# ID: 204742214, 004817743

ID = 204742214
FILES = lab3b.py Makefile README

default:
	echo 'python lab3b.py $$1' > lab3b
	chmod +x lab3b

dist:
	@tar -cvzf lab3b-$(ID).tar.gz $(FILES)

clean:
	rm -f lab3b-$(ID).tar.gz lab3b
