int maxProduct(int* nums, int numsSize) {
    int current,max_product,min_product,max,temp_max,temp_min;
        
        max_product=nums[0];
        min_product=nums[0];
        max=nums[0];
    for (int i =1;i<numsSize;i++)
    {
        current=nums[i];
        temp_max=maximum(current,safeMultiply(max_product,current),safeMultiply(min_product,current));
        temp_min=minimum(current,safeMultiply(max_product,current),safeMultiply(min_product,current));

        max_product=temp_max;
        min_product=temp_min;

        if(max_product>max)
        {
            max=max_product;
        }
    }
    return max;
}

int maximum(int a,int b,int c)
{
    return (a>b)?(a>c?a:c):(b>c?b:c);
}
int minimum(int a,int b, int c)
{
    return (a<b)?(a<c?a:c):(b<c?b:c);
}
int safeMultiply(int a, int b) {
    if (a > 0 && b > 0 && a > INT_MAX / b) return INT_MAX;
    if (a > 0 && b < 0 && b < INT_MIN / a) return INT_MIN;
    if (a < 0 && b > 0 && a < INT_MIN / b) return INT_MIN;
    if (a < 0 && b < 0 && a < INT_MAX / b) return INT_MAX;
    return a * b;
}