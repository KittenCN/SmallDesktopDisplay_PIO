import binascii
import os
import traceback
from PIL import Image
from io import BytesIO

curdir = "./src/Animate/"
os.chdir(curdir)

def processImage(in_file, saveImg=True):

    im = Image.open(in_file)
    # 截取文件名
    filename = in_file.split('.')[0]

    i = 0
    mypalette = im.getpalette()

    arr_name_all = ''  # 存取数组
    arr_size_all = ''  # 存储数组容量

    try:
        im.putpalette(mypalette)
        with open(filename + '.h', 'w', encoding='utf-8') as f:  # 写入文件
            f.write('#include <pgmspace.h> \n\n')
            while 1:
                print('.', end="")
                new_im = Image.new("RGB", im.size)
                new_im.paste(im)

                # 缩放图像，
                width = new_im.size[0]  # 获取原始图像宽度
                height = new_im.size[1]  # 获取原始图像高度
                new_height = 70  # 等比例缩放后的图像高度，根据实际需要调整
                print(width, " ", height)
                if height > new_height:
                    ratio = round(new_height / height, 3)  # 缩放系数
                    new_im = new_im.resize((int(width * ratio), int(height * ratio)), Image.ANTIALIAS)

                # 获取图像字节流，转16进制格式
                img_byte = BytesIO()  # 获取字节流
                new_im.save(img_byte, format='jpeg')
                # print(img_byte.getvalue())

                # 16进制字符串
                img_hex = binascii.hexlify(img_byte.getvalue()).decode('utf-8')  

                arr_name = filename + '_' + str(i)
                arr_size = 0  # 记录数组长度
                arr_name_all += arr_name + ','

                # 将ac --> 0xac
                f.write('const uint8_t ' + arr_name + '[] PROGMEM = { \n')  # 写前
                for index, x in zip(range(len(img_hex)), range(0, len(img_hex), 2)):
                    temp_hex = '0x' + img_hex[x:x + 2] + ', '
                    # 30个数据换行
                    if (index + 1) % 30 == 0:
                        temp_hex += '\n'

                    f.write(temp_hex)  # 写入文件
                    arr_size += 1
                f.write('\n};\n\n')  # 写结尾
                i += 1
                arr_size_all += str(arr_size) + ','

                # 保存一帧帧图像
                if saveImg:
                    if not os.path.exists('./out_img'):
                        os.mkdir('./out_img')
                    if not os.path.exists('./out_img/' + filename):
                        os.mkdir('./out_img/' + filename)
                    new_im.save('./out_img/' + filename + '/' + str(i) + '.jpg')

                try:
                    im.seek(im.tell() + 1)
                except EOFError:
                    # 动图读取结束
                    f.write('const uint8_t *' + filename + '[' + str(i) + '] PROGMEM { ' + arr_name_all + '};\n')
                    f.write('const uint32_t ' + filename + '_size[' + str(i) + '] PROGMEM { ' + arr_size_all + '};')
                    print("成功保存文件为：" + filename + '.h')
                    break

    except EOFError as e:
        print(e.args)
        print(traceback.format_exc())
        pass  # end of sequence

if __name__ == '__main__':
    processImage('miku.gif')