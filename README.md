# Pipelined_MIPS_with_cache
Pipelined MIPS processor with cache implemented in C

Multi-Cycle Pipelined MIPS에서 캐시 메모리를 추가로 구현한 코드로, 동일한 명령어와 파이프라인을 지원한다.<br>

<br>

***
<br>

## 메모리 계층 구조
컴퓨팅의 초창기부터 프로그래머들은 무제한의 크기를 갖는 매우 빠른 메모리를 원해왔다.<br>
비록 이를 만드는 것은 불가능하지만, 프로그래머들에게 무제한의 빠른 메모리를 갖고있는 듯한 환상을 만들어 주는 것은 가능하다.<br>
이러한 환상을 만들어 주기 위해서 프로그램이 동작하는 방법에 적용되는 ‘지역성의 원칙’을 이용한다. 지역성의 원칙의 예는 다음과 같다.<br>

어떤 사람이 주머니가 있는 옷과 가방을 메고 있다고 생각해보자. 주머니에 넣고 다니는 것들은 물건이 어디 있는지 빠르게 찾을 수 있지만 넣을 수 있는 개수가 한정적이다.<br>
반면에 가방의 경우, 많은 물건들을 넣을 수 있지만 주머니에 비해 찾는 시간이 오래 걸리게 된다.<br>
만약 이 사람이 핸드폰을 처음엔 가방에 넣고 다니다가, 사용할 일이 있어 가방에서 꺼내 주머니에 넣었다고 가정해보자.<br>
이 때, 가방에서 핸드폰만 꺼내는 것이 아니라 핸드폰과 관련된 블루투스 이어폰이나 보조배터리, 케이블과 같이 핸드폰에 관련된 것들을 같이 꺼내 주머니에 넣고 다닌다면, 다음에 핸드폰과 관련된 것들을 찾을 때 다시 가방을 찾아볼 일 없이 주머니를 찾는 것만으로도 이어폰을 연결해 음악을 듣거나, 보조배터리로 충전을 하는 일을 할 수 있게 된다. 이는 가방에서 핸드폰만 필요하다고 그것만 꺼내 주머니에 넣는 것보다 많은 시간을 절약하게 해준다.<br>

무제한의 빠른 메모리를 사용하는 환상을 만드는 데 이와 같은 원리가 이용된다.<br>
가방에 있는 물건들을 찾아볼 확률이 모두 똑같지 않듯이, 프로그램이 코드와 데이터에 접근할 확률이 모두 같지는 않다.<br>
또한 가방에 있는 모든 것들을 주머니에 넣고 저 사람이 필요로 하는 것을 빠르게 찾을 수 없듯이, 많은 정보량을 갖는 큰 메모리에서 메모리 접근을 빠르게 하는 것은 불가능하다.<br>
이처럼 어떤 순간에 사용하는 물건은 가방에 들어있는 물건들 중에서 극히 일부이듯이, 프로그램은 어떤 특정 시간에는 주소공간 내의 비교적 작은 부분에만 접근한다는 것이 지역성의 원칙이다.<br>

지역성에는 다음 두가지 종류가 있다.
- 시간적 지역성 : 어떤 항목이 참조되면, 곧바로 다시 참조되기가 쉽다. 위 예시의 사람이 길을 찾아보기 위해 핸드폰을 가방에서 주머니로 가져왔다면, 곧바로 핸드폰을 다시 찾게 될 가능성이 매우 크다.
- 공간적 지역성 : 어떤 항목이 참조되면, 그 근처에 있는 다른 항목들이 곧바로 참조될 가능성이 높다. 위 예시에서 핸드폰을 사용하기 위해 가방에서 핸드폰을 꺼냈다면, 핸드폰과 같이 넣어 놓았던 블루투스 이어폰, 보조배터리의 존재도 알게 될 것이고, 이 물건들 또한 핸드폰과 더불어 유용하게 사용될 것이다. 핸드폰과 관련된 것들은 공간적 지역성을 높이기 위해 가방에서 같은 공간에 함께 정리되어 있다.


<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/block.png?raw=true" width="50%"></center>
공간적 지역성을 최대한 활용하기 위하여 메모리 계층 간에 데이터를 복사할 때는 해당 데이터 하나만 복사하는 것이 아닌, 위 사진처럼 두 계층 간 데이터의 최소 단위인 line 전체를 복사하게 된다.


<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/locality.jpg?raw=true" width="50%"></center>
위 사진은 프로세스 실행 중 접근한 데이터의 접근 시점과 메모리 주소를 표현한 것으로, 시간적 지역성과 공간적 지역성을 잘 보여준다. 사진에서 가로 축은 실행 시간, 세로 축은 메모리 주소이다.<br>
수평으로 이어진 참조 기록은 긴 시간에 걸쳐 같은 메모리 주소를 참조한 것이고, 수직으로 이어진 참조 기록은 같은 시간에 밀접한 메모리 주소를 참조한 것이다.<br>

<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/hierarchy.png?raw=true" width="50%"></center>
지역성의 원칙을 활용하기 위해서 컴퓨터의 메모리는 계층구조를 이루는 방식으로 구현되어 있다. 메모리 계층구조는 서로 다른 속도와 크기를 갖는 여러 계층의 메모리로 구성되어 있다.<br>
가장 빠른 메모리는 더 느린 메모리보다 가격이 매우 비싸기 때문에 보통 그 크기가 작다.<br>

<br>
***
<br>

## 캐시
<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/m1%20die%20shot.png?raw=true" width="50%"></center>
위 사진은 Apple Silicon의 M1 프로세서의 die photography이다.<br>
M1 프로세서의 L1 캐시는 각 CPU Core의 내부에 위치해 있고, PERF CPU Core의 주변에 12MB L2 캐시, EFF CPU Core 주변에 4MB L2 캐시, 프로세서 중앙에 8MB System Level Cache, 즉 L3 캐시가 존재한다.<br>
이러한 캐시들은 SRAM으로 구성된 메모리로써 메모리 계층 구조에서 프로세서에 가장 가까이 위치한, 속도가 매우 빠른 메모리이다.<br>
캐시는 지역성의 원칙을 활용하여 메인 메모리에 있는 데이터를 저장해 놓고, 프로세서가 데이터를 요구했을 때 캐시에서 먼저 찾도록 하여 시스템 성능을 향상시킨다. 이 때, 요구한 데이터가 캐시에 존재하면 Cache HIT이라 부르고, 존재하지 않으면 Cache MISS라 부른다.<br>
Cache MISS가 난 이유가 만약 프로세서가 해당 Cache Line에 최초로 접근하여 그 line이 비어 있기 때문이라면 cold MISS, cache의 모든 way에서 프로세서가 요구하는 데이터의 주소에 해당하는 Cache Line이 다른 주소의 데이터로 이미 채워져 있는 상태라면 conflict MISS라 한다. 주머니로 예시를 들자면, 주머니가 비어 있는 상태에서 물건을 찾는 경우가 cold MISS, 주머니가 모두 차 있는데 찾는 물건이 주머니에 없는 경우가 conflict MISS이다.<br>
Cache MISS가 발생하면 요구하는 데이터를 포함하는 line을 찾기 위해 하위 계층 메모리인 메인 메모리에 접근하게 된다. 만약 메인 메모리에도 해당 line이 없다면, HDD, SSD와 같은 최하위 계층 메모리에 접근하게 된다.<br>

캐시의 성능을 평가하는 척도로 ‘캐시 적중률’을 사용한다. 캐시 적중률이 충분히 크다면 프로세서의 메모리 계층 접근 속도는 최상위 계층의 캐시와 비슷하게 되고, 메모리의 크기는 최하위 계층의 크기와 같아지므로 프로그래머에게 무제한의 빠른 메모리를 갖고 있는 듯한 환상을 보여줄 수 있게 된다.<br>
여기서 캐시는 하위 계층 메모리(메인 메모리)에 비해 크기가 작기 때문에 하위 계층의 모든 데이터를 캐시에 저장하여 캐시 적중률을 높일 수는 없다. 따라서 캐시에 데이터를 배치하는 방법을 정해두는 것이 필요하다.<br>

<br>
***
<br>

## 캐시 배치 정책
캐시를 구성하는 방식에 따라 캐시 메모리가 데이터를 저장하고 검색하는 방법, 장소가 결정된다. 캐시의 전반적인 성능은 보통 캐시 메모리의 논리적 구성의 영향을 많이 받게 된다.<br>
캐시 구성 방식으로는 크게 Direct Mapped Cache, Fully Associative Cache, Set Associative Cache의 3가지가 존재한다.<br>
캐시 구성 방식을 알아보기 전에 먼저 캐시 메모리의 논리적 기본 단위를 알아보도록 하자.<br>


### 캐시 메모리의 기본 단위
<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/cache%20address.png?raw=true" width="50%"></center>
위 사진은 32비트 주소체계를 가진 프로세서의 캐시 메모리이다.<br>
캐시의 엔트리가 1024개 존재하므로 Index비트 또한 그에 맞게 10개의 비트가 필요하다.<br>
Cache Line에서 offset비트가 2비트라 했을 때, 주소의 0~1번 비트가 Cache Line offset비트가 되고, 2~11번 비트가 Cache Line을 참조할 Index비트, 12~31번 비트가 캐시에 저장된 데이터의 주소를 식별할 Tag비트가 된다.<br>
Valid비트는 해당 Cache Line이 비어 있는지 혹은 사용 중 인지를 구별하기 위해 사용한다.<br>
프로세서가 특정 주소의 데이터를 요구했을 때 해당 주소의 Index비트를 캐시에서 참조하는데, 여기서 Valid비트가 0이라면 프로세서가 해당 주소의 데이터를 최초로 요구한 것이므로 cold MISS가 일어나게 된다.<br>
Valid비트가 1이면서 해당 주소의 Tag비트와 Cache Line의 Tag비트를 비교한 값이 다르다면 conflict MISS, 같다면 Cache HIT가 되어 주소의 offset에서 읽은 데이터를 프로세서에 넘겨준다.<br>
Cache MISS가 일어나면 지역성의 원칙에 의해 프로세서가 요구한 데이터만이 아닌, 주소의 line 전체를 메인 메모리에서 캐시로 가져오게 된다.<br>
위 사진을 바탕으로 설명하면, 만약 프로세서가 0x80000088 주소의 데이터를 요청하였고 Cache MISS가 일어났다면, 캐시에 메인 메모리의 0x80000088부터 0x8000008B까지의 데이터가 저장되는 방식이다. 다시 말해, offset비트가 모두 0 인 주소부터 모두 F인 주소까지의 데이터를 캐시로 가져오는 것이다.<br>


### Direct-Mapped Cache
<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/Direct%20Mapped.png?raw=true"></center>
Direct-Mapped Cache란 각 메모리의 위치가 캐시 내의 정확히 한 곳에만 사상 되는 캐시 구조를 말한다.<br>
위 사진에서 Cache Line 0은 메인 메모리 주소의 Index비트가 000000인 위치만 저장 될 수 있고, Cache Line 1은 000001, Cache Line 2는 000010... 이런 식으로 Cache Line 63까지 저장되어 메인 메모리에서 Block의 위치들이 캐시 내의 한 곳에만 저장될 수 있게 된다.<br>
Direct-Mapped Cache는 구조가 단순하면서도 속도가 매우 빠르다는 장점이 있지만, 여러 데이터가 캐시 의 동일한 위치에 저장되길 원하므로 conflict MISS가 발생할 확률이 높다는 단점이 있다. 이 문제를 해결하기 위해 나온 구성이 Fully-Associative Cache이다.<br>


### Fully-Associative Cache
<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/Fully%20Associative.png?raw=true"></center>
Fully-Associative Cache는 Direct-Mapped Cache와는 정반대로 각 메모리의 위치가 캐시 내의 아무 곳에나 사상 될 수 있다. 그러므로 메인 메모리의 데이터가 저장되는 캐시 위치가 정해져 있지 않으므로 Index비트를 사용하지 않고, Cache-Line offset비트를 제외한 모든 비트가 Tag비트로써 사용된다.<br>
따라서 conflict MISS가 발생할 확률이 높았던 Direct-Mapped Cache에 비해, Fully-Associative Cache에서는 Cache Line의 아무 곳이나 메인 메모리의 데이터가 저장될 수 있으므로 발생할 확률이 낮다.<br>
하지만 캐시에서 데이터를 찾을 때마다 모든 Cache Line에 대해 Tag비트를 검색해야 하므로 Direct-Mapped Cache에 비해 속도가 느리고 많은 전력을 소비하게 된다. 이런 Direct-Mapped Cache와 Fully-Associative Cache의 극단적인 장단점을 해결하기 위해 나타난 구성이 Set-Associative Cache이다.<br>


### Set-Associative Cache
<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/Set%20Associative%20Cache.jpg?raw=true" width="50%"></center>
Set-Associative Cache는 Direct-Mapped Cache와 Fully-Associative Cache를 적절히 혼합한 형태를 갖고 있다. Fully-Associative Cache처럼 아무 way에 메인 메모리의 데이터가 저장될 수 있고, Direct-Mapped Cache처럼 Index비트를 사용하여 way 내부에서 데이터가 저장될 Cache Line의 위치를 정한다.<br>
Set-Associative Cache의 way가 1개이면 Direct-Mapped Cache와 같고, way의 개수가 총 Cache Line의 개수와 같다면(각 way당 Cache Line의 개수가 1개라면) Fully-Associative Cache와 같다.<br>
캐시에서 데이터를 찾을 때 모든 way를 찾아야 하므로 속도는 Direct-Mapped Cache보다 느리지만, Index비트를 사용하므로 모든 Cache Line을 찾을 필요가 없어져 Fully-Associative Cache보다 더 빠른 속도와 낮은 전력을 소모하는 중간적인 특징을 갖고 있다. 이러한 특징 때문에 현대 CPU에서 통상적으로 사용하는 방식이다.<br>

<br>
***
<br>

## 캐시 교체 정책
Cache Line을 교체할 때 어떤 데이터를 버릴 것인지 정하는 것을 캐시 교체 정책이라 한다. 교체 정책에 따라 캐시가 더 효율적이게 사용될 수도 있고, 오히려 비효율적이게 사용될 수도 있다.<br>
Direct-Mapped Cache에선 Cache Line마다 저장할 수 있는 메인 메모리의 주소가 정해져 있으므로 사용하지 않는다.<br>
- LRU (Least Recently Used) : 가장 오랫동안 사용하지 않은 Cache Line을 교체하는 정책이다. LRU를 사용하면 캐시 적중률이 높게 유지되지만, 하드웨어적으로 구현이 어렵다.
- SCA (Second Chance Algorithm) : LRU를 하드웨어적으로 구현하기 위해 간소화한 정책이다.
- LFU (Least Frequently Used) : 가장 적게 사용한 Cache Line을 교체하는 정책이다. 하지만 최근에 메인 메모리에서 가져온 데이터가 삭제될 수도 있고, 프로그램 실행 초반에만 자주 쓰인 데이터가 마지막까지 잔존해 있을 수도 있다.
- FIFO (First In First Out) : 가장 오래된 Cache Line을 교체하는 정책이다. 이해하기 쉽고 구현이 간단하지만, 잦은 교체로 인해 실행속도를 떨어뜨릴 수 있다.
- Random : Cache Line을 무작위로 하나 선택하여 교체한다. 다른 교체 정책과 달리 과거 이력을 저장할 추가적인 유닛이 필요하지 않아 구조가 간단하다는 장점이 있다.

<br>
***
<br>

## 캐시 쓰기 정책
지금까지는 프로세서가 메모리로부터 값을 읽어오는 상황만을 생각하였지만, sw 명령어와 같이 프로세서가 메모리에 값을 쓰는 상황도 분명 존재한다. 이 때 캐시에서 쓰기를 처리하는 정책으로는 Write-through, Write-back이 있다.


### Write-through
<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/Write-through.png?raw=true"></center>
Write-through 정책은 프로세서가 메모리에 값을 작성할 때, 캐시와 메인 메모리 모두 값을 작성하는 방법이다.<br>
이 정책은 구조가 단순하여 구현이 쉽다는 장점이 있지만, 모든 쓰기 명령어에 대해 메인 메모리에 접근해야 한다는 단점이 있다. 메인 메모리에 접근하는 것은 캐시에 접근하는 것보다 훨씬 많은 Clock Cycle을 소모하므로, 결국 Write-through 정책에서 메모리에 값을 작성하는 일이 많아질수록 심한 성능 저하를 야기하게 된다.<br>


### Write-back
<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/Write-back.png?raw=true"></center>
Write-back 정책은 Write-through 정책의 대안으로, 프로세서가 메모리에 값을 작성할 때 캐시에만 값을 작성하고 메인 메모리엔 작성하지 않다가, 값이 수정된 Cache Line이 캐시 교체 정책에 의해 캐시에서 쫓겨날 때 메인 메모리에 해당 Cache line을 작성하는 정책이다.<br>
이 정책은 Write- through보다 높은 성능을 보여주지만, 구현하기가 더 어렵다.<br>

### 캐시 할당
만약 프로세서가 값을 캐시에 작성하려고 하는데, 캐시에 해당 Cache Line이 이미 교체되어 Cache MISS가 발생한 경우에는 어떻게 해야 할까?<br>
이 경우, 메인 메모리로부터 해당 line을 캐시로 가져와 값을 작성하는 Write allocate 방법과 메인 메모리에 해당 line만 수정하고 캐시에는 가져오지 않는 No write allocate 방법이 있다.<br>
Write-through 정책은 캐시와 메인 메모리 모두 값을 작성하므로 보통 No write allocate를 사용하고, Write-back 정책은 캐시에만 값을 작성하므로 보통 Write allocate 방법을 사용한다.<br>

<br>
***
<br>

## 구현
이 프로그램에서 구현한 캐시에서 캐시라인의 크기는 64bytes로 고정되어 있고, 캐시 메모리의 크기는 256, 512, 1024bytes 중 선택할 수 있다.<br>
Set-Associativity는 Direct-Mapped, 2-way, 4-way를 지원하고, 캐시 쓰기 정책은 Write-through, Write-back을 지원한다.<br>
캐시 교체 정책은 Least Recently Used(LRU)로 구현했으며, 구현 방식은 다음과 같다.<br>

<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/LRU%20shift%20register.png?raw=true" width="70%"></center>
LRU를 구현하기 위해 모든 way의 Index마다 위 사진처럼 shift register 역할을 하는 값을 할당하였다.<br>
이 값은 제일 오른쪽 비트의 Index 가 0이고 제일 왼쪽 비트의 Index가 2인 총 크기 3의 shift register를 표현한다.<br>
각 register는 해당 Index만큼 2의 거듭제곱한 값을 나타내며, shift register가 업데이트 될 때마다 오른쪽으로 한칸씩 이동한다.<br>
Input값은 제일 왼쪽부터 저장된다. 따라서 shift register의 모든 Index를 더한 값이 클수록 가장 최근에 사용한 way이고 작을수록 가장 오래 전에 사용한 way를 의미한다.<br>
프로그램에서 사용자가 선택할 수 있는 최대의 set이 4-way이기 때문에 shift register의 사이즈는 3으로 고정하였다. 이 값에 대한 예시로 4-way Set-Associative Cache에서 0번째 way의 3번 Index가 참조된 상황을 가정해보자.<br>

<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/LRU%201st.png?raw=true" width="70%"></center>
0번째 way의 3번 Index가 참조되었으므로 0번째 way의 3번 shift register[2]를 1로, 나머지 way들 의 shift register[2]는 0으로 저장한다.<br>
이 다음으로 3번째 way의 3번 Index가 참조되었다고 생각 해보자.<br>

<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/LRU%202nd.png?raw=true" width="70%"></center>
shift register의 값을 변경하기 전 오른쪽으로 한칸씩 값을 이동시킨다.<br>
3번째 way의 3번 Index가 참조되었으므로 3번째 way의 3번 shift register[2]를 1로, 나머지 way들의 shift register[2]는 0으로 저장한다.<br>
이 다음으로 2번째 way의 3번 Index가 참조되었다고 가정해보자.<br>

<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/LRU%203rd.png?raw=true" width="70%"></center>
이전과 동일하게 오른쪽으로 한칸씩 값을 이동한 후, 2번째 way에 3번 shift register[2]에는 1을, 나머지 way에는 0을 저장한다.<br>
이 다음으로 3번 Cache Line들 중 하나를 교체해야 하는 상황이 일어났다고 가정해보자.<br>

<center><img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/LRU%204th.png?raw=true" width="70%"></center>
3번 shift register의 값들 중 가장 작은 값이 1번째 way이므로 1번째 way의 3번 Cache Line이 새로운 데이터로 교체된다.<br>

LRU의 구현 방식을 제외한 
