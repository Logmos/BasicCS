unsigned short checksum(unsigned char *arr, int count)
{
    int sum = 0;
    while (count > 1) {
        sum += *(unsigned short *) arr;
        arr += 2;
        count -= 2;
    }   

    if (count > 0)
        sum += *(unsigned short *) arr;

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}
