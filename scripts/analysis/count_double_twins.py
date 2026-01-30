def count_double_twins(limit=12999):
    count = 0
    examples = []
    
    for i in range(limit + 1):
        s = str(i)
        # Check for xxyy pattern (adjacent pairs)
        has_double_twin = False
        
        # We need at least 4 characters to have two pairs
        if len(s) < 4:
            continue
            
        # Check xxyy
        for j in range(len(s) - 3):
            if s[j] == s[j+1] and s[j+2] == s[j+3] and s[j] != s[j+2]:
                has_double_twin = True
                break
        
        # Check xxzyy (only if xxyy not found)
        if not has_double_twin:
            for j in range(len(s) - 4):
                if s[j] == s[j+1] and s[j+3] == s[j+4] and s[j] != s[j+3]:
                    has_double_twin = True
                    break
        
        if has_double_twin:
            count += 1
            if len(examples) < 20:
                examples.append(s)
                
    print(f"Total numbers with Double Twin (xxyy) pattern in range [0, {limit}]: {count}")
    print(f"Examples: {examples}")

if __name__ == "__main__":
    count_double_twins()
