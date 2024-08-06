int maxArea(int* height, int heightSize) {
    //sort the array
    //two for loops
    //
    int max_value=0,left=0,right=heightSize-1,area;
    while(left<right)
    {
        area=(right-left)*(minimum(height[left],height[right]));
        max_value=maximum(area,max_value);

        if(height[left]<height[right])
        {
            left++;
        }
        else
        {
            right--;
        }
    }
    return max_value;
}

int maximum(int a, int b)
{
    return a>b?a:b;
}
int minimum(int a, int b)
{
    return a<b?a:b;
}