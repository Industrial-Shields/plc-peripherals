#!/usr/bin/env python3
import sys

def decode_config(cfg):
    cfg &= 0xFFFF
    b = lambda hi,lo: (cfg >> lo) & ((1 << (hi-lo+1)) - 1)
    return {
        'raw': f"0x{cfg:04X}",
        'OS': b(15,15),
        'MUX': b(14,12),
        'PGA': b(11,9),
        'MODE': b(8,8),
        'DR': b(7,5),
        'COMP_MODE': b(4,4),
        'COMP_POL': b(3,3),
        'COMP_LAT': b(2,2),
        'COMP_QUE': b(1,0),
    }

def interpret_fields(d, mode):
    # mode: 'W' or 'R'
    mux_map = {
        0b000: "AIN0-AIN1 (diff) (default)",
        0b001: "AIN0-AIN3 (diff)",
        0b010: "AIN1-AIN3 (diff)",
        0b011: "AIN2-AIN3 (diff)",
        0b100: "AIN0 single-ended",
        0b101: "AIN1 single-ended",
        0b110: "AIN2 single-ended",
        0b111: "AIN3 single-ended",
    }
    pga_map = {
        0b000: "±6.144 V",
        0b001: "±4.096 V",
        0b010: "±2.048 V (default)",
        0b011: "±1.024 V",
        0b100: "±0.512 V",
        0b101: "±0.256 V",
    }
    dr_map = {
        0b000: "128 SPS",
        0b001: "250 SPS",
        0b010: "490 SPS",
        0b011: "920 SPS",
        0b100: "1600 SPS (default)",
        0b101: "2400 SPS",
        0b110: "3300 SPS",
        0b111: "3300 SPS",
    }
    comp_que_map_write = {
        0b00: "Assert after one conversion",
        0b01: "Assert after two conversions",
        0b10: "Assert after four conversions",
        0b11: "Disable comparator (default)",
    }
    comp_que_map_read = {
        0b00: "ALERT/RDY triggers after one conversion",
        0b01: "ALERT/RDY triggers after two conversions",
        0b10: "ALERT/RDY triggers after four conversions",
        0b11: "Comparator disabled, ALERT/RDY high-impedance (default)",
    }

    res = {}
    # OS: different meaning W vs R
    if mode == 'W':
        res['OS'] = f"{d['OS']}  — {'Start single conversion (when writing)' if d['OS']==1 else 'No effect when writing'}"
    else:
        res['OS'] = f"{d['OS']}  — {'Conversion complete (when reading) (default)' if d['OS']==1 else 'Conversion in progress (when reading)'}"

    res['MUX'] = f"{d['MUX']:03b}  — {mux_map.get(d['MUX'], 'Reserved/Unknown')}"
    res['PGA'] = f"{d['PGA']:03b}  — {pga_map.get(d['PGA'], 'Reserved/Unknown')}"
    res['MODE'] = f"{d['MODE']}  — {'Continuous-conversion (0)' if d['MODE']==0 else 'Single-shot / power-down (1) (default)'}"
    res['DR'] = f"{d['DR']:03b}  — {dr_map.get(d['DR'], 'Unknown')}"
    res['COMP_MODE'] = f"{d['COMP_MODE']}  — {'Traditional comparator (0) (default)' if d['COMP_MODE']==0 else 'Window comparator (1)'}"
    res['COMP_POL'] = f"{d['COMP_POL']}  — {'Active low (0) (default)' if d['COMP_POL']==0 else 'Active high (1)'}"
    res['COMP_LAT'] = f"{d['COMP_LAT']}  — {'Non-latching (0) (default)' if d['COMP_LAT']==0 else 'Latching (1)'}"

    # COMP_QUE semantics differ subtly for write (setting) vs read (status of ALERT/RDY)
    if mode == 'W':
        res['COMP_QUE'] = f"{d['COMP_QUE']:02b}  — {comp_que_map_write.get(d['COMP_QUE'], 'Unknown')}"
    else:
        res['COMP_QUE'] = f"{d['COMP_QUE']:02b}  — {comp_que_map_read.get(d['COMP_QUE'], 'Unknown')}"

    return res

def pretty_print(orig, interp):
    print(f"Config register: {orig['raw']}\n")
    print(f"OS (bit 15): {interp['OS']}")
    print(f"MUX (14-12): {interp['MUX']}")
    print(f"PGA (11-9): {interp['PGA']}")
    print(f"MODE (8): {interp['MODE']}")
    print(f"DR (7-5): {interp['DR']}")
    print(f"COMP_MODE (4): {interp['COMP_MODE']}")
    print(f"COMP_POL (3): {interp['COMP_POL']}")
    print(f"COMP_LAT (2): {interp['COMP_LAT']}")
    print(f"COMP_QUE (1-0): {interp['COMP_QUE']}")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: ads1015_config_decoder.py <config_int> <W|R>")
        sys.exit(1)
    s, mode = sys.argv[1], sys.argv[2].upper()
    if mode not in ('W','R'):
        print("Mode must be 'W' or 'R'.")
        sys.exit(1)
    try:
        val = int(s,0)
    except ValueError:
        print("Invalid integer.")
        sys.exit(1)
    decoded = decode_config(val)
    interpreted = interpret_fields(decoded, mode)
    pretty_print(decoded, interpreted)
