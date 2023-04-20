
شبکه بی سیم
================================


## مقدمه

<div dir="rtl">وجود چند نگارنده با داشتن نگاشت های مخصوص باعث می شود سیستمی غیر متمرکز برای رمز نگاری انجام شود و هیچ نودی به تنهایی رمز ندارد و در صورت همکاری همه نود های لازم فقط میتوانیم رمزگشایی کنیم
      در این پروژه میخواهیم یک شبکه بیسیم‌
‌را‌با‌استفاده‌از‌ابزار‌شبیهسازی‌3-ns‌شبیه‌سازی‌کنیم‌و‌به‌تحلیل‌معی‌ارهای‌‌مختلف‌آن‌بپردازیم.‌این‌
شبکه‌یک سیستم رمزنگاری ساده‌را‌پ‌یادهسازی میکند.
</div>

## طراحی شبکه
<div dir="rtl">
در بخش‌های بعدی به ترتیب نحوه‌ی طراحی شبکه را توضیح می‌دهیم.
در ارتباط بین کلاینت و مستر ار ارتباط udp و در ارتباط مستر و نگارنده از tcp و در نهایت با اتصال udp کاراکتر های رمز 
   گشایی شده را برای کلاینت در همان بستر udp ارسال میکند.
    برای نگاشت از 3 نگارنده استفاده میشود که هریک اطلاعات به خصوصی را دارند و امر نگاشتن بدون همکاری همه آنها امکان پذیر نیست.
</div>


## کلاس کلاینت
<div dir="rtl">با کد های زیر با استفاده از interfaceContainer به مستر کانکت شدیم این کار در startApplication انجام شده در generateTraffic نیز داده های تصادفی توسط socket کانکت شده در قسمت قبلی گفته شده شروع به ایجاد داده های تصادفی بین 0 تا 25 می  کند 
    پ
    بدیهی است که برای امکان اتصال از طرف یک نگارنده باید  interfaceContainer دیگری نیز داشته باشم که روی آن دریافت با ایجاد سوکت 
   را انجام دهیم
و در کلاس های بعدی و کد های بعدی نمونه کدی برای دریافت و سپس و هم ارسال را انجام دهد
</div>

```c++
static void GenerateTraffic(Ptr<Socket> socket, uint16_t data)
{
    Ptr<Packet> packet = new Packet();
    MyHeader m;
    m.SetData(data);

    packet->AddHeader(m);
    packet->Print(std::cout);
    socket->Send(packet);

    Simulator::Schedule(Seconds(0.1), &GenerateTraffic, socket, rand() % 26);
}

void client::StartApplication(void)
{
    Ptr<Socket> sock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress sockAddr(ip.GetAddress(0), port);
    sock->Connect(sockAddr);

    GenerateTraffic(sock, 0);
}
```

## کلاس مستر
<div dir="rtl">در اینجا با داشتن 2  interfaceContainer که یکی مربوط به آدرسی است که باید روی آن bind کنیم ودیگری آدرسی است که باید به آن connect شویم.
در اینجا هم برای ایجاد ترافیک باید تابعی داشته باشیم اما در اینجا با کلاس کلاینت تفاتی دارد که باید همان داده دریافتی و آدرس او را بفرستد. این امر در تابع sendDataMasterToMapper آمده است.</div>

```c++
void master::SendDataMasterToMapper(Ptr<Socket> socket, uint16_t data)
{
    Ptr<Packet> packet = new Packet();
    MyHeader m;
    m.SetData(data);

    packet->AddHeader(m);
    packet->Print(std::cout);
    socket->Send(packet);
}

void master::StartApplication(void)
{
    socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(ip.GetAddress(0), port);
    socket->Bind(local);

    mapper_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    InetSocketAddress sockAddr(targetIp.GetAddress(0), port);
    mapper_socket->Connect(sockAddr);

    socket->SetRecvCallback(MakeCallback(&master::HandleRead, this));
}
```
## کلاس نگارنده
<div dir="rtl">در این کلاس ما با دریافت داده ها آنها را نگاشت میکنیم این رمزنگاری به صورت کد سخت انجام شده وجود چند نگارنده با داشتن نگاشت های مخصوص باعث می شود سیستمی غیر متمرکز برای رمز نگاری انجام شود و هیچ نودی به تنهایی رمز ندارد و در صورت همکاری همه نود های لازم فقط میتوانیم رمزگشایی کنیم.
در ادامه کدهایی میبینیم برای اتصال به مستر و دریافت اطلاعات از آن که با interface مستر socket که میسازد به آن گوش میسپارد تا داده ها را بگیرد.</div>
```c++
void mapper::StartApplication(void)
{
    socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(ip.GetAddress(0), port);
    socket->Bind(local);
    socket->Listen();

    socket->SetRecvCallback(MakeCallback(&mapper::HandleRead, this));
}
```