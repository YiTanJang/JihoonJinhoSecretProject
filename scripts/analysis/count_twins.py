def get_span(s, max_len):
    results = set()
    q = [(s[i], i) for i in range(len(s))]
    
    head = 0
    while head < len(q):
        curr_s, idx = q[head]
        head += 1
        results.add(curr_s)
        
        for ni in [idx - 1, idx + 1]:
            if 0 <= ni < len(s):
                next_s = curr_s + s[ni]
                if len(next_s) <= max_len:
                    q.append((next_s, ni))
    return results

def has_twin(s):
    for i in range(len(s) - 1):
        if s[i] == s[i+1]:
            return True
    return False

def main():
    covered = set()
    basis = set()
    limit_len = 5
    
    # 1 to 12,000
    for i in range(1, 12001):
        s = str(i)
        if s in covered:
            continue
        
        span = get_span(s, limit_len)
        for item in span:
            covered.add(item)
            
        # Remove any existing basis strings that are now covered by this larger string
        basis = {b for b in basis if b not in span}
        basis.add(s)

    counts = {3: 0, 4: 0, 5: 0}
    twins = {3: 0, 4: 0, 5: 0}
    
    for s in basis:
        l = len(s)
        if l in counts:
            counts[l] += 1
            if has_twin(s):
                twins[l] += 1
                
    print(f"Total Basis Strings: {len(basis)}")
    for l in [3, 4, 5]:
        if counts[l] > 0:
            print(f"Len {l}: {counts[l]} (Twins: {twins[l]}, {twins[l]/counts[l]*100:.1f}%)")
        else:
            print(f"Len {l}: 0 (Twins: 0, 0.0%)")
    
    total_twins = sum(twins.values())
    print(f"Total Twins: {total_twins} ({total_twins/len(basis)*100:.1f}%)")

if __name__ == "__main__":
    main()
