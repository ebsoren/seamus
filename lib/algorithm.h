
template<class It, class T>
bool binary_search(It first, It last, T& value) {

    while (first < last) {
        It mid = first + (last - first) / 2;

        if (!(*mid < value) && !(value < *mid)) return true;

        if  (*mid < value) {
            first = mid + 1;
        } else {
            last = mid;
        }
    }
    
    return false;
}