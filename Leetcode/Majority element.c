int majorityElement(int* nums, int numsSize) {
    uint numbers[256]={0};
/*
    for(int i =0;i<numsSize;i++)
    {
        numbers[nums[i]]++;
        if (numbers[nums[i]]>(numsSize/2))
        {
            return nums[i];
        }
    }
    return 0;
    */
    //boyr moore volting algorithm
    int count =0;
    int candidate;

    for(int i =0;i<numsSize;i++)
    {
        if (count==0)
        {
            candidate=nums[i];
        }
        count+=candidate==nums[i]?1:-1;
    }

    count=0;
    for(int i =0;i<numsSize;i++)
    {
        if (nums[i]==candidate)
        {
            count++;
              if (count>numsSize/2)
             {
                return candidate;
             }
        }
    }

  


    return -1;
}