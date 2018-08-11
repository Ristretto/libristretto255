from collections import namedtuple
from binascii import unhexlify

comb_config = namedtuple("comb_config",["n","t","s"])
wnaf_config = namedtuple("wnaf_config",["fixed","var"])

field_data = {
    "p25519" : {
        "gf_desc" : "2^255 - 19",
        "gf_shortname" : "25519",
        "gf_impl_bits" : 320,
        "gf_lit_limb_bits" : 51,
        "elligator_onto" : 0
    },
}

curve_data = {
    "curve25519" : {
        "altname" : "IsoEd25519",
        "iso_to" : "Curve25519",
        "name" : "Ristretto",
        "cofactor" : 8,
        "field" : "p25519",
        "scalar_bits" : 253,
        "d": -121665,
        "trace": -0xa6f7cef517bce6b2c09318d2e7ae9f7a,
        "mont_base": 9,
        "rist_base": "e2f2ae0a6abc4e71a884a961c500515f58e30b6aa582dd8db6a65945e08d2d76",
        "eddsa_encode_ratio": 4,
        "x_encode_ratio": 4,
        
        "combs":comb_config(3,5,17),
        "wnaf":wnaf_config(5,3),
        "window_bits":4,

        "eddsa_sigma_iso": 1
    },
}

def ser(x,bits,paren=None):
    out = ""
    mask = 2**bits - 1
    first = True
    while x > 0 or first:
        desc = "0x%0*x" % ((bits+3)//4,x&mask)
        if paren is not None:
            desc = "%s(%s)" % (paren,desc)
        if not first: out += ", "
        out += desc
        x = x >> bits
        first = False
    return out

def msqrt(x,p,hi_bit_clear = True, lo_bit_clear = False):
    if p % 4 == 3: ret = pow(x,(p+1)//4,p)
    elif p % 8 == 5:
        for u in range(1,1000):
            if pow(u,(p-1)//2,p) != 1: break
        u = pow(u,(p-1)//4,p)
        ret = pow(x,(p+3)//8,p)
        if pow(ret,2,p) != (x % p): ret = (ret * u) % p
    else: raise Exception("sqrt only for 3-mod-4 or 5-mod-8")
        
    if (ret**2-x) % p != 0: raise Exception("No sqrt")
    if hi_bit_clear and ret > p//2: ret = p-ret
    # lo_bit_clear overrides hi_bit_clear because it's not default
    if lo_bit_clear and (ret & 1): ret = p-ret 
    return ret
        
def ceil_log2(x):
    out = 0
    cmp = 1
    while x > cmp:
        cmp = cmp<<1
        out += 1
    return out

for field,data in field_data.items():
    if "modulus" not in data:
        data["modulus"] = eval(data["gf_desc"].replace("^","**"))
    
    if "gf_bits" not in data:
        data["gf_bits"] = ceil_log2(data["modulus"])

for curve,data in curve_data.items():
    for key in field_data[data["field"]]:
        if key not in data:
            data[key] = field_data[data["field"]][key]

        
    if "iso_to" not in data:
        data["iso_to"] = data["name"]
        
    if "cxx_ns" not in data:
        data["cxx_ns"] = data["name"].replace("-","")
    
    if "rist_base_decoded" not in data:
        def xord(x):
            if isinstance(x,str): return ord(x)
            else: return x
        data["rist_base_decoded"] = sum(
                xord(b)<<(8*i) for i,b in enumerate(unhexlify(data["rist_base"]))
            )

    if "imagine_twist" not in data:
        # This is a HACK.  The real problem is that iso-Ed25519
        # has points at infinity unless you IMAGINE_TWIST.
        #
        # Also there are lots of bugs when cofactor=8 != IMAGINE_TWIST.
        # (FUTURE: fix all this to support other curves, eventually)
        if data["modulus"]%4 == 3: data["imagine_twist"] = 0
        else: data["imagine_twist"] = 1
        # data["imagine_twist"] = 0

    data["q"] = (data["modulus"]+1-data["trace"]) // data["cofactor"]
    data["bits"] = ceil_log2(data["modulus"])
    
    if "c_ns" not in data:
        data["c_ns"] = "decaf_" + str(data["bits"])
        data["C_NS"] = data["c_ns"].upper()
