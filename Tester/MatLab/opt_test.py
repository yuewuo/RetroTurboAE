from numpy.linalg import cond
from scipy.linalg import circulant

from numpy import sqrt,random
from scipy import optimize

from numpy import array
import numpy as np


def function(a):
    return cond(circulant(a));
def step(a : array):
    s = random.randint(0,len(a),size=2)
    a[s[0]], a[s[1]] = a[s[1]], a[s[0]]
    return a



#use BFGS algorithm for optimization
if __name__ == "__main__":
    a = array([2,0,0,0,0,0,2,2,0,0,2,0,2,2,0,2,2,2,2,0,2,0,2,0,0,0,2,0,0,2,2,2])
    #print(cond(circulant(a),2))
    print(optimize.(function, a,  take_step=step, disp=True))


