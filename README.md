# Pipelined_MIPS_with_cache
Pipelined MIPS processor with cache implemented in C

Multi-Cycle Pipelined MIPS에서 캐시 메모리를 추가로 구현한 코드로, 동일한 명령어와 파이프라인을 지원한다.

<img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/m1%20die%20shot.png?raw=true">

위 사진은 Apple Silicon의 M1 프로세서의 die photography이다.
M1 프로세서의 L1 캐시는 각 CPU Core의 내부에 위치해 있고, PERF CPU Core의 주변에 12MB L2 캐시, EFF CPU Core 주변에 4MB L2 캐시, 프로세서 중앙에 8MB System Level Cache, 즉 L3 캐시가 존재한다.
이러한 캐시들은 SRAM으로 구성된 메모리로써 메모리 계층 구조에서 프로세서에 가장 가까이 위치한, 속도가 매우 빠른 메모리이다.
캐시는 지역성의 원칙을 활용하여 메인 메모리에 있는 데이터를 저장해 놓고, 프로세서가 데이터를 요구했을 때 캐시에서 먼저 찾도록 하여 시스템 성능을 향상시킨다.

이 프로그램에서 구현한 캐시에서 캐시라인의 크기는 64bytes로 고정되어 있고, 캐시 메모리의 크기는 256, 512, 1024bytes 중 선택할 수 있다.
Set-Associativity는 Direct-Mapped, 2-way, 4-way를 지원하고, 캐시 쓰기 정책은 Write-through, Write-back을 지원한다.

캐시 교체 정책은 Least Recently Used(LRU)로 구현했으며, 구현 방식은 다음과 같다.

<img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/LRU%20shift%20register.png?raw=true">

LRU를 구현하기 위해 모든 way의 Index마다 위 사진처럼 shift register 역할을 하는 값을 할당하였다.
이 값은 제일 오른쪽 비트의 Index 가 0이고 제일 왼쪽 비트의 Index가 2인 총 크기 3의 shift register를 표현한다.
각 register는 해당 Index만큼 2의 거듭제곱한 값을 나타내며, shift register가 업데이트 될 때마다 오른쪽으로 한칸씩 이동한다.
Input값은 제일 왼쪽부터 저장된다. 따라서 shift register의 모든 Index를 더한 값이 클수록 가장 최근에 사용한 way이고 작을수록 가장 오래 전에 사용한 way를 의미한다.
프로그램에서 사용자가 선택할 수 있는 최대의 set이 4-way이기 때문에 shift register의 사이즈는 3으로 고정하였다. 이 값에 대한 예시로 4-way Set Associative Cache에서 0번째 way의 3번 Index가 참조된 상황을 가정해보자.

<img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/LRU%201st.png?raw=true">

0번째 way의 3번 Index가 참조되었으므로 0번째 way의 3번 shift register[2]를 1로, 나머지 way들 의 shift register[2]는 0으로 저장한다.
이 다음으로 3번째 way의 3번 Index가 참조되었다고 생각 해보자.

<img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/LRU%202nd.png?raw=true">

shift register의 값을 변경하기 전 오른쪽으로 한칸씩 값을 이동시킨다.
3번째 way의 3번 Index가 참조되었으므로 3번째 way의 3번 shift register[2]를 1로, 나머지 way들의 shift register[2]는 0으로 저장한다.
이 다음으로 2번째 way의 3번 Index가 참조되었다고 가정해보자.

<img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/LRU%203rd.png?raw=true">

이전과 동일하게 오른쪽으로 한칸씩 값을 이동한 후, 2번째 way에 3번 shift register[2]에는 1을, 나머지 way에는 0을 저장한다.
이 다음으로 3번 캐시라인들 중 하나를 교체해야 하는 상황이 일어났다고 가정해보자.

<img src="https://github.com/SNMac/Pipelined_MIPS_with_cache/blob/master/LRU%204th.png?raw=true">

3번 shift register의 값들 중 가장 작은 값이 1번째 way이므로 1번째 way의 3번 캐시라인이 새로운 데이터로 교체된다.

LRU의 구현 방식을 
