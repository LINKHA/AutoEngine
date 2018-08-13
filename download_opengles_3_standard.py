"""*************************************************************
*downloading glad for Opengl es 3.0 version
"""
import sys
import urllib.request

print ("downloading with urllib")
url = 'https://glad.dav1d.de/generated/tmp1wNkmbglad/glad.zip'
f = urllib.request.urlopen(url)
data = f.read()
with open(sys.path[0]+"/glad.zip", "wb") as code:
    code.write(data)
print ("Finish")