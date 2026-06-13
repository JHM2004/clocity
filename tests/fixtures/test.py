import os


# This is a comment
def greet(name):
    # Another comment
    msg = "hello " + name
    url = "http://example.com"
    doc = """triple quoted"""
    return msg
# A third comment
# A fourth comment
x = greet("world")  # mixed line
print(x)
result = x + "!"
