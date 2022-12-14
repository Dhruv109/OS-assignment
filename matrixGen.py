import numpy as np

LONG_LONG_MAX =  9223372036854775807


n = int(input("I: "))
m = int(input("J: "))
k = int(input("K: "))
#f =  open("Matrix_out.txt")
mat = np.random.randint(0,1000,size=(n,m))
np.savetxt("in1.txt",mat,fmt='%i')
mat2 = np.random.randint(0,1000,size=(m,k))
np.savetxt("in2.txt",mat2,fmt='%i')
matRes = np.dot(mat,mat2)   
np.savetxt("matrixres.txt",matRes,fmt='%i')

