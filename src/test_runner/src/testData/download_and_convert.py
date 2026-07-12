import urllib.request
import re
import os

def download_file(url, dest):
    print(f"Downloading {url} to {dest}...")
    try:
        # User Agent header to avoid block
        req = urllib.request.Request(
            url, 
            headers={'User-Agent': 'Mozilla/5.0'}
        )
        with urllib.request.urlopen(req) as response:
            with open(dest, 'wb') as f:
                f.write(response.read())
        print(f"Successfully downloaded {dest}")
        return True
    except Exception as e:
        print(f"Failed to download {url}: {e}")
        return False

def parse_matpower_matrix(content, name):
    pattern = re.compile(rf"mpc\.{name}\s*=\s*\[(.*?)\]\s*;", re.DOTALL)
    match = pattern.search(content)
    if not match:
        return []
    
    matrix_str = match.group(1)
    lines = matrix_str.split("\n")
    rows = []
    for line in lines:
        line = line.split("%")[0].strip()
        if not line:
            continue
        elements = re.split(r"[\s,\t;]+", line)
        elements = [e for e in elements if e]
        if elements:
            rows.append(elements)
    return rows

def convert_m_to_xml(m_file_path, xml_file_path, name):
    print(f"Converting {m_file_path} to {xml_file_path}...")
    if not os.path.exists(m_file_path):
        print(f"Error: {m_file_path} does not exist.")
        return
        
    with open(m_file_path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()
        
    buses = parse_matpower_matrix(content, "bus")
    branches = parse_matpower_matrix(content, "branch")
    gens = parse_matpower_matrix(content, "gen")
    
    if not buses or not branches or not gens:
        print(f"Failed to parse matrices for {name}.")
        return

    # XML Writing
    with open(xml_file_path, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write(f'<!-- Generated IEEE {name} test case for Vjetroelektrana DFIG plugin -->\n')
        f.write(f'<PowerSystem name="{name}" baseMVA="100">\n\n')
        
        # Defaults
        f.write('  <DFIGDefaults xs="0.01" xmu="3.5" Htotal="3.5" Teps="0.01"\n')
        f.write('                KV="10.0" vref="1.0" Popt="0.5" OmegaB="314.159"\n')
        f.write('                omega_m0="1.012" i_rq0="-0.15" i_rd0="0.05"/>\n\n')
        
        f.write('  <StdGenDefaults xd_prime="0.0608" H="23.64" D="0.0"\n')
        f.write('                  OmegaB="314.159" E_prime="1.05"\n')
        f.write('                  delta0="0.0" omega0="1.0"/>\n\n')
        
        # Buses
        f.write('  <Buses>\n')
        for b in buses:
            # bus_i, type, Pd, Qd, Gs, Bs, area, Vm, Va, baseKV
            bus_num = b[0]
            b_type = b[1]
            pd = b[2]
            qd = b[3]
            gs = b[4]
            bs = b[5]
            vm = b[7]
            va = b[8]
            base_kv = b[9] if len(b) > 9 else "138.0"
            f.write(f'    <Bus number="{bus_num}" type="{b_type}" Pd="{pd}" Qd="{qd}" Gs="{gs}" Bs="{bs}" baseKV="{base_kv}" Vm="{vm}" Va="{va}"/>\n')
        f.write('  </Buses>\n\n')
        
        # Branches
        f.write('  <Branches>\n')
        for br in branches:
            # f_bus, t_bus, br_r, br_x, br_b, rate_a, ..., ratio, angle
            from_b = br[0]
            to_b = br[1]
            r = br[2]
            x = br[3]
            b = br[4]
            rate = br[5]
            ratio = br[8] if len(br) > 8 else "0"
            shift = br[9] if len(br) > 9 else "0"
            # In MATPOWER transformer ratio defaults to 0 if not transformer
            if float(ratio) == 0.0:
                ratio = "0"
            f.write(f'    <Branch from="{from_b}" to="{to_b}" r="{r}" x="{x}" b="{b}" rateA="{rate}" tap="{ratio}" shift="{shift}"/>\n')
        f.write('  </Branches>\n\n')
        
        # Generators (we automatically designate the 2nd generator as DFIG, or the 1st if only 1 exists)
        f.write('  <Generators>\n')
        dfig_index = 1 if len(gens) > 1 else 0
        for idx, g in enumerate(gens):
            # bus, Pg, Qg, Qmax, Qmin, Vg, mBase
            bus = g[0]
            pg = g[1]
            qg = g[2]
            qmax = g[3]
            qmin = g[4]
            vs = g[5]
            mbase = g[6] if len(g) > 6 else "100.0"
            
            f.write(f'    <Gen bus="{bus}" number="{idx+1}" Pg="{pg}" Qg="{qg}" Qmax="{qmax}" Qmin="{qmin}" Vs="{vs}" mBase="{mbase}">\n')
            
            if idx == dfig_index:
                # DFIG Node
                f.write('      <DFIG xs="0.01" xmu="3.5" Htotal="3.5" Teps="0.01"\n')
                f.write(f'            KV="10.0" vref="{vs}" Popt="0.85" OmegaB="314.159"\n')
                f.write('            omega_m0="1.012" i_rq0="-0.24" i_rd0="0.02"/>\n')
            else:
                # Standard Gen Classical Model
                f.write('      <StdGen xd_prime="0.0608" H="23.64" D="0.0"\n')
                f.write('              OmegaB="314.159" E_prime="1.05"\n')
                f.write('              delta0="0.0" omega0="1.0"/>\n')
            f.write('    </Gen>\n')
        f.write('  </Generators>\n\n')
        f.write('</PowerSystem>\n')
    print(f"Successfully converted {xml_file_path}")

def run():
    dest_dir = "/home/abu/SREES/src/test_runner/src/testData"
    os.makedirs(dest_dir, exist_ok=True)
    
    cases = {
        "case14": "https://raw.githubusercontent.com/MATPOWER/matpower/master/data/case14.m",
        "case39": "https://raw.githubusercontent.com/MATPOWER/matpower/master/data/case39.m",
        "case145": "https://raw.githubusercontent.com/MATPOWER/matpower/master/data/case145.m",
        "case2383wp": "https://raw.githubusercontent.com/MATPOWER/matpower/master/data/case2383wp.m",
        "case3120sp": "https://raw.githubusercontent.com/MATPOWER/matpower/master/data/case3120sp.m"
    }
    
    for name, url in cases.items():
        m_file = os.path.join(dest_dir, f"{name}.m")
        xml_file = os.path.join(dest_dir, f"{name}.xml")
        if download_file(url, m_file):
            convert_m_to_xml(m_file, xml_file, name)
            # Remove .m file to keep testData clean
            if os.path.exists(m_file):
                os.remove(m_file)

if __name__ == "__main__":
    run()
