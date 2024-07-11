bool canPlaceFlowers(int* flowerbed, int flowerbedSize, int n) {
 for (int i = 0; i < flowerbedSize; i++) {
        if (flowerbed[i] == 0) {
            bool emptyLeft = (i == 0 || flowerbed[i - 1] == 0);
            bool emptyRight = (i == flowerbedSize - 1 || flowerbed[i + 1] == 0);

            if (emptyLeft && emptyRight) {
                flowerbed[i] = 1;
                n--;
                if (n == 0) {
                    return true;
                }
            }
        }
    }
    return n <= 0;
}