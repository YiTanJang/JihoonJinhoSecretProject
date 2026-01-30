import collections

def analyze_unbiased_score(present_numbers):
    # present_numbers: set of integers (0-9999)
    
    # 1. Bucket Definitions
    # buckets[d] = { 'candidates': set(), 'capacity': 10000 }
    buckets = collections.defaultdict(lambda: {'candidates': set(), 'capacity': 1000})
    
    # Populate initial buckets
    for num in present_numbers:
        d = num // 1000
        buckets[d]['candidates'].add(num)
        
    print("--- Initial State ---")
    for d in range(10):
        print(f"Bucket {d}: {len(buckets[d]['candidates'])} candidates")

    # 2. Filter Trivial Bounces (ABAC / CABA type)
    # A path A->B->A->C is physically traversing edge (A,B) then (B,A) then (A,C).
    # In terms of digits d1, d2, d3, d4: 
    # If d1 == d3, it's a bounce at d2? (A->B->A->C : d1=A, d2=B, d3=A. Yes.)
    # If d2 == d4, it's a bounce at d3? (C->A->B->A : d2=A, d3=B, d4=A. Yes.)
    
    trivial_removed = 0
    for num in list(present_numbers):
        d1 = num // 1000
        d2 = (num // 100) % 10
        d3 = (num // 10) % 10
        d4 = num % 10
        
        is_bounce = (d1 == d3) or (d2 == d4)
        
        if is_bounce:
            # Remove from bucket candidates AND capacity
            # Wait, if we remove from capacity, we are saying "This number DOES NOT COUNT towards the score potential".
            # If it's present, we remove it. What if it's NOT present?
            # To unbiasedly reduce capacity, we must reduce it for ALL theoretically possible bounce numbers.
            # But here we only see "present" numbers. 
            # We need to iterate 0000-9999 to calculate correct capacities first.
            pass

    # RESTART: Correct Logic for Capacities
    # We iterate 0..9999. For each number, we determine its type and ownership.
    
    # Track ownerships
    # ownership[num] = 'bucket_X' or 'shared_X_Y' or 'trivial'
    ownership = {}
    
    for val in range(10000):
        d1 = val // 1000
        d2 = (val // 100) % 10
        d3 = (val // 10) % 10
        d4 = val % 10
        
        # 1. Trivial Bounces
        if (d1 == d3) or (d2 == d4):
            ownership[val] = 'trivial'
            continue
            
        # 2. Palindromes (d1==d4 and d2==d3)
        if (d1 == d4) and (d2 == d3):
            ownership[val] = f'bucket_{d1}_pal'
            continue
            
        # 3. Cyclic (d1 == d4) but not palindrome
        if (d1 == d4):
            # It pairs with its reverse.
            # Reverse of AXYZ -> XZYA. (Wait, reverse of A->B->C->A is A->C->B->A)
            # Val: d1 d2 d3 d4. Reverse: d4 d3 d2 d1.
            # Since d1=d4, Reverse is d1 d3 d2 d1.
            rev_val = d4*1000 + d3*100 + d2*10 + d1
            
            # We enforce a canonical order to handle the pair once
            if val < rev_val:
                ownership[val] = f'bucket_{d1}_cyclic_primary'
                ownership[rev_val] = f'bucket_{d1}_cyclic_secondary'
            elif val == rev_val:
                # Should be covered by Palindrome check, but just in case
                ownership[val] = f'bucket_{d1}_pal'
            continue
            
        # 4. Standard (d1 != d4)
        # Pairs with d4 d3 d2 d1 (Bucket d4)
        rev_val = d4*1000 + d3*100 + d2*10 + d1
        ownership[val] = f'shared_{d1}_{d4}' # Owned by d1, claims against d4

    # Calculate Capacities and Initial Fills
    bucket_stats = collections.defaultdict(lambda: {'capacity': 0, 'fill': 0, 'raw_fill': 0})
    
    # First pass: Determine Capacities (Max possible score)
    # And count Raw Fills (to determine sort order)
    
    for val in range(10000):
        owner = ownership[val]
        present = (val in present_numbers)
        
        if owner == 'trivial':
            # Completely ignored. Does not contribute to capacity or fill.
            continue
            
        if '_pal' in owner:
            # Palindrome: Counts 1 towards Capacity, 1 towards Fill
            d = int(owner.split('_')[1])
            bucket_stats[d]['capacity'] += 1
            if present: 
                bucket_stats[d]['fill'] += 1
                bucket_stats[d]['raw_fill'] += 1
                
        elif '_cyclic_primary' in owner:
            # Primary Cyclic: Counts 1 towards Capacity, 1 towards Fill
            d = int(owner.split('_')[1])
            bucket_stats[d]['capacity'] += 1
            if present:
                bucket_stats[d]['fill'] += 1
                bucket_stats[d]['raw_fill'] += 1
                
        elif '_cyclic_secondary' in owner:
            # Secondary Cyclic: 
            # "remove every second counted numbers... decrease bucket size by 1"
            # This means it DOES NOT count towards Capacity.
            # And if present, it DOES NOT count towards Fill.
            # Effectively ignored, same as trivial.
            # BUT, we might want to check if Primary is missing but Secondary is present?
            # "AXYA... AYXA". Physically they are the same loop. 
            # If AXYA is missing but AYXA is present, we should count it!
            # Let's refine: A cyclic pair (A, B) contributes 1 to Capacity.
            # Fill = 1 if (A in present OR B in present).
            d = int(owner.split('_')[1])
            # We handle this by checking the pair in the Primary step.
            pass
            
        elif 'shared_' in owner:
            parts = owner.split('_')
            d1, d4 = int(parts[1]), int(parts[2])
            # Shared: Counts 1 towards Capacity of d1. 
            # (And the reverse counts 1 towards Capacity of d4).
            # So Val adds to d1. Rev_Val adds to d4.
            bucket_stats[d1]['capacity'] += 1
            if present:
                bucket_stats[d1]['fill'] += 1
                bucket_stats[d1]['raw_fill'] += 1

    # Fix Cyclic Logic: Check pairs
    for val in range(10000):
        if '_cyclic_primary' in ownership[val]:
            d = int(ownership[val].split('_')[1])
            d1, d2, d3, d4 = val//1000, (val//100)%10, (val//10)%10, val%10
            rev_val = d4*1000 + d3*100 + d2*10 + d1
            
            is_present = (val in present_numbers)
            is_rev_present = (rev_val in present_numbers)
            
            # If BOTH are present, we counted 'fill' in the loop above.
            # But we need to ensure we don't double count if we change logic.
            # Current logic above: Primary adds 1 if present. Secondary adds 0.
            # YOUR REQUIREMENT: "make sure every bucket size also decrease by 1"
            # This implies if we have 2 candidates (Primary, Secondary), max score is 1.
            # Capacity is already 1 (only Primary counted).
            # What if Primary is missing but Secondary is present?
            # We should probably credit that.
            if not is_present and is_rev_present:
                bucket_stats[d]['fill'] += 1 # Credit for the loop existing
                bucket_stats[d]['raw_fill'] += 1

    # 3. Sort Buckets by Raw Fullness
    # "lexicographically order the buckets by bucket size" (meaning fullness count)
    ranked_buckets = sorted(range(10), key=lambda d: bucket_stats[d]['raw_fill'], reverse=True)
    print(f"Bucket Priority: {ranked_buckets}")

    # 4. Debias Shared Numbers (Type 3)
    # "erase the latter number... decrease the bucket size"
    
    # We need to iterate through pairs again.
    processed_pairs = set()
    
    for val in range(10000):
        if 'shared_' not in ownership[val]: continue
        
        d1 = val // 1000
        d4 = val % 10
        rev_val = (val%10)*1000 + ((val//10)%10)*100 + ((val//100)%10)*10 + (val//1000)
        
        pair_id = tuple(sorted((val, rev_val)))
        if pair_id in processed_pairs: continue
        processed_pairs.add(pair_id)
        
        # Determine Winner and Loser
        # Ranking: Index in ranked_buckets (0 is best/first, 9 is worst/last)
        rank_d1 = ranked_buckets.index(d1)
        rank_d4 = ranked_buckets.index(d4)
        
        winner_digit = -1
        loser_digit = -1
        loser_val = -1
        
        if rank_d1 < rank_d4: # d1 comes first (better)
            winner_digit = d1
            loser_digit = d4
            loser_val = rev_val # The number belonging to d4
        else:
            winner_digit = d4
            loser_digit = d1
            loser_val = val # The number belonging to d1
            
        # "erase the latter number (from candidates) ... decrease size (capacity) by 1"
        # 1. Decrease Loser Capacity
        bucket_stats[loser_digit]['capacity'] -= 1
        
        # 2. Remove Loser Fill (if it was counted)
        if loser_val in present_numbers:
            bucket_stats[loser_digit]['fill'] -= 1
            # We don't touch raw_fill as that was used for sorting only
            
    # Final Report
    print("\n--- Final Unbiased Stats ---")
    print(f"{ 'Digit':<6} | {'Capacity':<10} | {'Fill':<10} | {'%':<8}")
    print("-" * 40)
    
    total_score = 0
    for i, d in enumerate(ranked_buckets):
        cap = bucket_stats[d]['capacity']
        fill = bucket_stats[d]['fill']
        pct = (fill / cap * 100) if cap > 0 else 0
        print(f"{d:<6} | {cap:<10} | {fill:<10} | {pct:5.1f}%")
        
        # Weighted Score Calculation
        weight = (10 - i) ** 4
        total_score += (fill / cap) * weight if cap > 0 else 0
        
    print(f"\nTotal Unbiased Score: {total_score:.4f}")

# Example Usage
if __name__ == "__main__":
    # Mock data: create a set of numbers that includes some bounces, cyclics, etc.
    # 1212 (Cyclic), 1221 (Cyclic Rev), 1234 (Shared), 4321 (Shared Rev)
    # 1213 (Bounce at 2), 1231 (Standard Cyclic)
    
    # Test Set: {1219, 1221, 1231, 1321, 1234, 1235, 1236, 4321, 4235, 4334, 5321, 5324, 6321, 9121}
    # 1219: Bounce (d1=1, d3=1). Ignore.
    # 9121: Bounce (d2=1, d4=1). Ignore.
    # 1221: Palindrome. Bucket 1.
    # 1231/1321: Cyclic. Bucket 1.
    mock_present = set([
        1219, 1221, 1231, 1321, 1234, 1235, 1236, 
        4321, 4235, 4334, 
        5321, 5324, 
        6321, 
        9121
    ])
    
    # We expect Bucket 1 to have some cyclic capacity/fill.
    # Bucket 4 to lose capacity because 1234 exists (assuming 1 is prioritized?).
    # Actually priority depends on raw counts. 
    
    analyze_unbiased_score(mock_present)
