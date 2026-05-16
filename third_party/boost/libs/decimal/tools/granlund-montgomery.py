def extended_euclidean(a, b):
    if a == 0:
        return b, 0, 1
    gcd, x1, y1 = extended_euclidean(b % a, a)
    x = y1 - (b // a) * x1
    y = x1
    return gcd, x, y

def mod_inverse(a, m):
    gcd, x, _ = extended_euclidean(a, m)
    if gcd != 1:
        return 0  # Modular inverse doesn't exist
    else:
        return x % m

# Constants
bits = int(128)
t = int(32)

q = int(10)**t
q0 = int(q / int(2)**t)
print("Q0: ", q0)
twobt_min_t = int(2**(bits - t))

# Calculate the modular inverse
m0 = int(mod_inverse(q0, twobt_min_t))
print("M0: ", m0)

p0 = int((q0 * m0 - 1) / twobt_min_t)
print("P0: ", p0)

p = int(q0 + p0)
print("P: ", p)

m = int((twobt_min_t * p + 1) / q0)
print("M: ", m)

threshold_value = int(2**bits / q + 1)
print("Threshold Value: ", threshold_value)
