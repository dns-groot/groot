import subprocess
import logging
import json
import sys
import argparse

# Setting up a logger
level = logging.INFO
logger = logging.getLogger('logger')
logger.setLevel(level)
ch = logging.StreamHandler()
ch.setLevel(level)
formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
ch.setFormatter(formatter)
logger.addHandler(ch)

# Some global variables
query_name = ''
save_dir = '.'
visited_ns = []
visited_domain = []

def parse(zonefile):
    records = []
    lines = zonefile.rstrip().split("\n")
    for line in lines:
        if line.rstrip() != "" and line[0]!=';':
            rr = line.split()
            records.append(rr)
    return records

def dump(name, zonefile, ns, metadata):
    filename = name+'.txt'
    abs_filename = os.path.join(save_dir, filename)
    with open(abs_filename, mode='w') as f:
        f.write(zonefile)
        logger.debug("dump(): dumped to file "+name+'.txt')
    metadata['ZoneFiles'].append({'FileName':name+'.txt', 'NameServer':ns})

def zone_transfer(domain, ns, metadata, c, top):
    
    if ns not in visited_ns:
        visited_ns.append(ns)
    else:
        return

    logger.info('dig axfr '+domain+' @'+ns)
    process = subprocess.Popen(['dig', 'axfr', domain, '@'+ns],
                                stdout = subprocess.PIPE,
                                stderr = subprocess.PIPE,
                                universal_newlines=True)
    stdout, stderr = process.communicate()
    if stderr != "":
        logger.error("Encountered an error while running dig: " + stderr)
        return False

    name = domain + '-' + str(c)
    c += 1
    records = parse(stdout)
    soa_mname = ''

    if records != []:
        for record in records:
            if record[3] == 'SOA':
                if top == True:
                    soa_mname = record[4]
                    if soa_mname not in metadata['TopNameServers']:
                        metadata['TopNameServers'].append(soa_mname)
                if soa_mname in visited_domain:
                    return
                else:
                    visited_ns.append(soa_mname)
                
    if records != []:
        if soa_mname != '':
            dump(name, stdout, soa_mname, metadata)
        else:
            dump(name, stdout, ns, metadata)
        for record in records:
            if record[3] == 'NS':
                zone_transfer(domain, record[4], metadata, c, False)
            if record[3] == 'CNAME':
                download(record[4], metadata, False)
        return True
    else:
        return False

def download(domain, metadata, top):
    
    if domain in visited_domain:
        return
    else:
        visited_domain.append(domain)
    
    logger.info('dig +short ns '+domain)
    process = subprocess.Popen(['dig', '+short', 'ns', domain],
                    stdout = subprocess.PIPE,
                    stderr = subprocess.PIPE,
                    universal_newlines = True)

    stdout, stderr = process.communicate()

    if stderr != "":
        logger.error("Encountered an error while running dig: " + stderr)
        return

    nameservers = stdout.rstrip().split("\n")
    for ns in nameservers:
        if ns.rstrip() == '':
            continue
        logger.info('trying to transfer from '+ns)
        if top == True:
            success = zone_transfer(domain, ns, metadata, 0, True)
        else:
            success = zone_transfer(domain, ns, metadata, 0, False)
        if success == True:
            logger.info('zone transfer successful: '+ns)
            return
        logger.info('zone transfer failed: '+ns)

def main():
    parser = argparse.ArgumentParser(description='Download a set of zone files')
    parser.add_argument('domain_name', metavar='NAME', type=str,
                    help='The domain name you are querying')
    parser.add_argument('save_directory', metavar='DIR', type=str,
                    help='Where you want to save your zone files')
    args = parser.parse_args()
    query_name = args.domain_name
    save_dir = args.save_directory

    # Testing #############
    # d1 = "ucla.edu"
    # d2 = "zonetransfer.me"
    # query_name = d2
    # save_dir = '.'
    #######################

    metadata = {'TopNameServers':[], 'ZoneFiles':[]}
    download(query_name, metadata, True)
    with open(save_dir+'/'+'metadata.json', mode='w') as metafile:
        metafile.write(json.dumps(metadata, indent=4))

if __name__ == "__main__":
    main()
