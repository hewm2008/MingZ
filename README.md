# mingz
<b>mingz: An ultra-fast gzip file tools</b>
<br>mingz: An ultra-fast gzip file tool that supports multithreading, etc., is faster than both pigz and gzip in comprehensive testing
##  1. Install
### (1) Building from source （Linux or Mac）
```
git clone https://github.com/hewm2008/MingZ.git
cd MingZ
make
cd bin
./mingz -h
```
## 2. Usage
```
	Usage: mingz  big1.fq *.fq
	Options:
		-i   <str>   input of text file or pipe
		-o   <str>   output of text file
		-t           number of threads [4]
		-c           write on standard output
		-d           decompress(no threads)
		-h           show help hewm2008[v1.10]
```
## 3. Example
### 3.1 
```
	mingz  InA.fq  InB.fq  -o NewOutA.fq.gz   -o NewOutB.fq.gz 
	mingz *.fq   # will remove raw file out InA.fq.gz InB.fq.gz
	mingz  InA.fq  -t  10  # muti threads
	cat  InA.fq  | mingz  > NewOutA.fq.gz  ##    cat  InA.fq  | mingz  -o  NewOutA.fq.gz
```
### 3.2 decompress File
```
	mingz In.fq.gz -c -d  > new.Fq
	mingz -i In.Fa.gz -o  OutNew.fq  
	mingz *.gz -d   # will remove raw file
```
## 4. accuracy and efficiency
An ultra-fast  fast >pigz > gzip ,see follows:
![test.png](https://github.com/hewm2008/MingZ/blob/main/include/test.png)
## 5. License
------------
  This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
### 6 Contact
------------
- [:email:](https://github.com/hewm2008/NGenomeSyn) hewm2008@gmail.com / hewm2008@qq.com
- join the<b><i> QQ Group : 125293663</b></i>
