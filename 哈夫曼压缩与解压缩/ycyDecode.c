#pragma pack(push)
#pragma pack(1)		//内存对其改为1个字节对齐模式

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ycy.h"

typedef struct HUF_FILE_HEAD {
	unsigned char flag[3];				//压缩二进制文件头部标志 ycy
	unsigned char alphaVariety;		//字符种类
	unsigned char lastValidBit;		//最后一个字节的有效位数
	unsigned char unused[11];			//待用空间
} HUF_FILE_HEAD;								//这个结构体总共占用16个字节的空间

typedef struct ALPHA_FREQ {
	unsigned char alpha;		//字符,考虑到文件中有汉字，所以定义成unsigned char
	int freq;								//字符出现的频度
} ALPHA_FREQ;

typedef struct HUFFMAN_TAB {
	ALPHA_FREQ alphaFreq;
	int leftChild;
	int rightChild;
	boolean visited;
	char *code;
} HUFFMAN_TAB;

boolean isFileExist(char *fileName);
ALPHA_FREQ *getAlphaFreq(char *sourceFileName, int *alphaVariety, HUF_FILE_HEAD fileHead);
void showAlphaFreq(ALPHA_FREQ *alphaFreq, int alphaVariety);
HUFFMAN_TAB *initHuffmanTab(ALPHA_FREQ *alphaFreq, int alphaVariety, int *hufIndex);
void destoryHuffmanTab(HUFFMAN_TAB *huffmanTab, int alphaVariety);
void showHuffmanTab(HUFFMAN_TAB *huffmanTab, int count);
int getMinFreq(HUFFMAN_TAB *huffmanTab, int count);
void creatHuffmanTree(HUFFMAN_TAB *huffmanTab, int alphaVariety);
void makeHuffmanCode(HUFFMAN_TAB *huffmanTab, int root, int index, char *code);
HUF_FILE_HEAD readFileHead(char *sourceFileName);
void huffmanDecoding(HUFFMAN_TAB *huffmanTab, char *sourceFileName, char *targetFileName, int alphaVariety, HUF_FILE_HEAD fileHead);

void huffmanDecoding(HUFFMAN_TAB *huffmanTab, char *sourceFileName, char *targetFileName, int alphaVariety, HUF_FILE_HEAD fileHead) {
	int root = 2 * alphaVariety - 2;
	FILE *fpIn;
	FILE *fpOut;
	boolean finished = FALSE;
	unsigned char value;
	unsigned char outValue;
	int index = 0;
	long fileSize;
	long curLocation;

	fpIn = fopen(sourceFileName, "rb");
	fpOut = fopen(targetFileName, "wb");
	fseek(fpIn, 0L, SEEK_END);
	fileSize = ftell(fpIn);	//文件总长度fileSize
	fseek(fpIn, 16 + 5 * fileHead.alphaVariety, SEEK_SET);	//略过前面16个字节的元数据，5字节的字符种类和频度
	curLocation = ftell(fpIn);
	
	//从根出发，'1'向左子树走，'0'向右子树走，若到达叶子结点，输出叶子结点下标对应的字符。再回到根结点继续。
	fread(&value, sizeof(unsigned char), 1, fpIn);
	while(!finished) {
		if(huffmanTab[root].leftChild == -1 && huffmanTab[root].rightChild == -1) {
			outValue = huffmanTab[root].alphaFreq.alpha;
			fwrite(&outValue, sizeof(unsigned char), 1, fpOut);
			if(curLocation >= fileSize && index >= fileHead.lastValidBit) {
				break;
			} 
			root = 2 * alphaVariety - 2;
		}
		
		//取出的一个字节从第一位开始看，'1'向左子树走，'0'向右子树走
		//若超过一个字节，8位，则需要读取下一个字节
		if(GET_BYTE(value, index)) {
			root = huffmanTab[root].leftChild;
		} else {
			root = huffmanTab[root].rightChild;
		}
		if(++index >= 8) {
			index = 0;
			fread(&value, sizeof(unsigned char), 1, fpIn);
			curLocation = ftell(fpIn);
		}
	}

	fclose(fpIn);
	fclose(fpOut);
}

HUF_FILE_HEAD readFileHead(char *sourceFileName) {
	HUF_FILE_HEAD fileHead;
	FILE *fp;

	fp = fopen(sourceFileName, "rb");
	//读取压缩的文件的头部元数据，16个字节
	fread(&fileHead, sizeof(HUF_FILE_HEAD), 1, fp);
	fclose(fp);

	return fileHead;
}

void makeHuffmanCode(HUFFMAN_TAB *huffmanTab, int root, int index, char *code) {
	if(huffmanTab[root].leftChild != -1 && huffmanTab[root].rightChild != -1) {
		code[index] = '1';
		makeHuffmanCode(huffmanTab, huffmanTab[root].leftChild, index + 1, code);
		code[index] = '0';
		makeHuffmanCode(huffmanTab, huffmanTab[root].rightChild, index + 1, code);
	} else {
		code[index] = 0;
		strcpy(huffmanTab[root].code, code);
	}
}

void creatHuffmanTree(HUFFMAN_TAB *huffmanTab, int alphaVariety) {
	int i;
	int leftChild;
	int rightChild;

	//huffmanTab使用剩下的 alphaVariety - 1个空间
	for(i = 0; i < alphaVariety - 1; i++) {
		leftChild = getMinFreq(huffmanTab, alphaVariety + i);
		rightChild = getMinFreq(huffmanTab, alphaVariety + i);
		huffmanTab[alphaVariety + i].alphaFreq.alpha = '#';
		huffmanTab[alphaVariety + i].alphaFreq.freq = huffmanTab[leftChild].alphaFreq.freq + huffmanTab[rightChild].alphaFreq.freq;
		huffmanTab[alphaVariety + i].leftChild = leftChild;
		huffmanTab[alphaVariety + i].rightChild = rightChild;
		huffmanTab[alphaVariety + i].visited = FALSE;
	}
}

//在哈夫曼表中找没有访问过的最小频度下标
int getMinFreq(HUFFMAN_TAB *huffmanTab, int count) {
	int index;
	int minIndex = NOT_INIT;

	for(index = 0; index < count; index++) {
		if(FALSE == huffmanTab[index].visited) {
			if(NOT_INIT == minIndex || huffmanTab[index].alphaFreq.freq < huffmanTab[minIndex].alphaFreq.freq) {
				minIndex = index;
			}
		}
	}
	huffmanTab[minIndex].visited = TRUE;

	return minIndex;
}

void showHuffmanTab(HUFFMAN_TAB *huffmanTab, int count) {
	int i;

	printf("%-4s %-4s %-4s %-6s %-6s %-6s %s\n", "下标", "字符", "频度", "左孩子", "右孩子", "visited", "code");
	for (i = 0; i < count; i++) {
		printf("%-5d %-4c %-5d %-6d %-7d %-4d %s\n",
				i,
				huffmanTab[i].alphaFreq.alpha,
				huffmanTab[i].alphaFreq.freq,
				huffmanTab[i].leftChild,
				huffmanTab[i].rightChild,
				huffmanTab[i].visited,
				(huffmanTab[i].code ? huffmanTab[i].code : "无"));
	}
}

void destoryHuffmanTab(HUFFMAN_TAB *huffmanTab, int alphaVariety) {
	int i;

	for(i = 0; i < alphaVariety; i++) {
		free(huffmanTab[i].code);
	}

	free(huffmanTab);
}

HUFFMAN_TAB *initHuffmanTab(ALPHA_FREQ *alphaFreq, int alphaVariety, int *hufIndex) {
	int i;
	HUFFMAN_TAB *huffmanTab = NULL;

	huffmanTab = (HUFFMAN_TAB *) calloc(sizeof(HUFFMAN_TAB), 2 * alphaVariety - 1);
	//huffmanTab申请了 2 * alphaVariety - 1大小的空间，在这只用了 alphaVariety个，还剩alphaVariety - 1个
	for(i = 0; i < alphaVariety; i++) {
		hufIndex[alphaFreq[i].alpha] = i;	//把哈夫曼表中的字符和其对应的下标形成键值对,存到hufIndex中
		huffmanTab[i].alphaFreq = alphaFreq[i];
		huffmanTab[i].leftChild = huffmanTab[i].rightChild = -1;
		huffmanTab[i].visited = FALSE;
		huffmanTab[i].code = (char *) calloc(sizeof(char), alphaVariety);
	}

	return huffmanTab;
}

void showAlphaFreq(ALPHA_FREQ *alphaFreq, int alphaVariety) {
	int i;

	for(i = 0; i < alphaVariety; i++) {
		int ch = alphaFreq[i].alpha;
		//字符超出了ASCII码表示范围
		if(ch > 127) {
			printf("[%d]: %d\n", ch, alphaFreq[i].freq);
		} else {
			printf("[%c]: %d\n", ch, alphaFreq[i].freq);
		}
	}
}

ALPHA_FREQ *getAlphaFreq(char *sourceFileName, int *alphaVariety, HUF_FILE_HEAD fileHead) {
	int freq[256] = {0};
	int i;
	int index;
	ALPHA_FREQ *alphaFreq = NULL;
	FILE *fpIn;
	int ch;

	*alphaVariety = fileHead.alphaVariety;
	alphaFreq = (ALPHA_FREQ *) calloc(sizeof(ALPHA_FREQ), *alphaVariety);
	fpIn = fopen(sourceFileName, "rb");
	//略过前16个字节的元数据
	fseek(fpIn, 16, SEEK_SET);
	fread(alphaFreq, sizeof(ALPHA_FREQ), *alphaVariety, fpIn);
	fclose(fpIn);

	return alphaFreq;
}

boolean isFileExist(char *fileName) {
	FILE *fp;

	fp = fopen(fileName, "rb");
	if (NULL == fp) {
		return FALSE;
	}

	fclose(fp);
	return TRUE;
}

int main(int argc, char const *argv[]) {
	char sourceFileName[256] = {0};
	char targetFileName[256] = {0};
	ALPHA_FREQ *alphaFreq = NULL;				//统计字符及频度的数组
	int alphaVariety = 0;								// 字符种类
	HUFFMAN_TAB *huffmanTab = NULL;			//哈夫曼表
	char *code = NULL;									//存储字符的哈夫曼编码
	int hufIndex[256] = {0};						//下标为字符的ASCII码，其值为该字符在哈夫曼表中的下标，形成键值对
	HUF_FILE_HEAD fileHead;

	if(argc != 3) {
		printf("正确命令格式: ycyDeCode <源文件名> <目标文件名>\n");
		return 0;
	}

	//第二个参数为源文件名
	strcpy(sourceFileName, argv[1]);
	if(!isFileExist(sourceFileName)) {
		printf("源文件(%s)不存在！\n", sourceFileName);
		return 0;
	}
	fileHead = readFileHead(sourceFileName);
	if(!(fileHead.flag[0] == 'y' && fileHead.flag[1] == 'c' && fileHead.flag[2] == 'y')) {
		printf("不可识别的文件格式\n");
	}
	//第三个参数为目标文件名
	strcpy(targetFileName, argv[2]);

	alphaFreq = getAlphaFreq(sourceFileName, &alphaVariety, fileHead);
	//showAlphaFreq(alphaFreq, alphaVariety);

	huffmanTab = initHuffmanTab(alphaFreq, alphaVariety, hufIndex);
	creatHuffmanTree(huffmanTab, alphaVariety);
	code = (char *) calloc(sizeof(char), alphaVariety);
	makeHuffmanCode(huffmanTab, 2 * alphaVariety - 2, 0, code);
	//showHuffmanTab(huffmanTab, 2 * alphaVariety - 1);

	huffmanDecoding(huffmanTab, sourceFileName, targetFileName, alphaVariety, fileHead);

	destoryHuffmanTab(huffmanTab, alphaVariety);
	free(alphaFreq);
	free(code);

	return 0;
}

#pragma pack(pop)