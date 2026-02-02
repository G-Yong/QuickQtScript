#ifndef MYSCRIPTCODE_H
#define MYSCRIPTCODE_H

#include <QString>

QString defultcode =
R"(
function add(a, b){
    return a + b;
}

var val = add(12, 3)
console.log(val)

var c
var d = 1
var e

if(true){
 console.log(true)
}
if(false){
 console.log(false)
}

switch(d){
case 0:
     console.log('this is', val)
break;
case 1:
     console.log('this is', val)
break;
case 2:
     console.log('this is', val)
break;
default:break;
}

var offset = 0
for(var i=0; i<50; i++)  {
   offset++
   sleep(1000)
   console.log('offset', offset)
   sleep(1000)
}
/*
sleep(1000)
console.log('result', add(12, 3))
sleep(1000)
console.log('done')

var val = 0
//while(1)
{
    sleep(1000)
    console.log(Date.now())

    sleep(1000)

    val++
    if(val >= 2)
    {
        val = 0;
    }
}
*/
)";

QString asnyccode =
R"(
var a = [1, 2, 3]
console.log(a)

async function f() {
  return 'hello world';
}

f().then(v => console.log(v))
Promise.resolve().then(() => {
  console.log('B')
})
console.log('A')

let count = 0;
const intervalId = setInterval(() => {
    count++;
    console.log(`这是第 ${count} 次输出，时间：`, new Date().toLocaleTimeString());

    // 运行5次后停止
    if (count >= 5) {
        clearInterval(intervalId);
        console.log('定时器已停止');
    }
}, 1000); // 1000毫秒 = 1秒

count = 0;
function printMessage() {
    count++;
    console.log(`这是第 ${count} 次输出，时间：`, new Date().toLocaleTimeString());

    // 设置下一次执行
    if (count < 5) {
        setTimeout(printMessage, 1000);
    } else {
        console.log('循环结束');
    }
}
printMessage()
)";

QString debugcode =
R"(
// ==================== import外部模块测试 ====================
console.log("\n===== import外部模块测试 =====");
import {modulePrint as Aprint} from './moduleA.js'
import {modulePrint as Bprint} from './moduleB.js'
Aprint();
Bprint();
// ==================== import配置模块测试 ====================
console.log("\n===== import配置模块测试 =====");
import {Print, obj} from 'm';
Print('直接导入配置模块m的Print函数和对象obj'); // 直接调用模块的Print
console.log(Print)
console.log(obj)  // 直接访问对象

import * as m from 'm';
m.Print('导入配置模块 m '); // 调用模块的Print函数
console.log(m.int32)
console.log(m.int64)
console.log(m.double)
console.log(m.str)
console.log(m.obj.str) // 通过模块前缀来访问对象
console.log(m.obj)
// ==================== Set/Map测试 ====================
console.log("\n===== Set/Map测试 =====");
var set = new Set([1, '2', 1]);
console.log(set);

var map = new Map([
    [1, "one"],
    [2, "two"],
    [1, "uno"]
]);
console.log(map);

const setData = [[1, 2], [3, 4]];
const ws = new WeakSet(setData);
console.log(ws);	// { < WeakSet > }

const mapKey1 = [1, 2, 3];
const mapKey2 = [4, 5, 6];
const wm2 = new WeakMap([[mapKey1, 'foo'], [mapKey2, 'bar']]);
console.log(wm2); // { < WeakMap > }
// ==================== Iterator测试 ====================
console.log("\n===== Iterator测试 =====");
var objIter = {a:1, b:'JIC', c:true}
console.log(Object.entries(objIter))

var setIter = new Set([1, 2, 3]);
console.log(setIter.entries())

var mapIter = new Map();
mapIter.set(1, 'JIC');
mapIter.set(2, 'PLC');
console.log(mapIter.entries())

var arrIter = ['JIC', 'PLC']
console.log(arrIter.entries())

const strIt = "A\uD835\uDC68";
const strIter = strIt[Symbol.iterator]();
console.log(strIter)
// ==================== Symbol测试 ====================
console.log("\n===== Symbol测试 =====");
let s1 = Symbol('foo');
let s2 = Symbol('bar');
console.log(s1) // Symbol(foo)
console.log(s2) // Symbol(bar)
console.log(s1.toString()) // "Symbol(foo) "
console.log(s2.toString()) // "Symbol(bar) "
// ==================== QObject测试 ====================
console.log("\n===== QObject测试 =====");
console.log("传入外部QObject");
console.log("对象名:", qObj.objectName);
qObj.newProp = "new prop";
console.log("访问属性:", qObj.newProp);
qObj.fun = () => {
    return "new function";
};
console.log("调用方法:", qObj.fun());
console.log("打印对象:", qObj);
qObj.deleteLater();
console.log("deleteLater()已调用");
// ==================== 无参函数签名测试 ====================
console.log("\n===== 无参函数签名测试 =====");
console.log('callPure->', callPure(666));
// ==================== 构造函数测试 ====================
console.log("\n===== 构造函数测试 =====");
var a = new Foo();
console.log('a.bar=', a.bar);
var b = Foo();
console.log('b.baz=', b.baz);
// ==================== 原型注册测试 ====================
console.log("\n===== 原型注册测试 =====");
var c = new Bar();
// console.log(typeof c.greet === 'function'); // true
console.log('c.greet=', typeof c.greet === 'function' ? c.greet() : c.greet);
var d = Bar();
// console.log(typeof Bar.prototype.greet === 'function'); // true
console.log('Bar.prototype.greet=', typeof Bar.prototype.greet === 'function' ? Bar.prototype.greet() : Bar.prototype.greet);
// ==================== 变量测试 ====================
// 基本变量声明和赋值
var intVar = 42;
var floatVar = 3.14159;
var strVar = "Hello, QuickJS!";
var boolVar = true;
var nullVar = null;
var undefinedVar;

console.log("\n===== 变量测试 =====");
console.log("整数:", intVar);
console.log("浮点数:", floatVar);
console.log("字符串:", strVar);
console.log("布尔值:", boolVar);
console.log("null值:", nullVar);
console.log("undefined值:", undefinedVar);

// 变量类型检查
console.log("intVar的类型:", typeof intVar);
console.log("floatVar的类型:", typeof floatVar);
console.log("strVar的类型:", typeof strVar);

// ==================== 数组测试 ====================
console.log("\n===== 数组测试 =====");

// 数组创建和访问
var arr = [1, 2, 3, 4, 5];
console.log("数组:", arr);
console.log("数组长度:", arr.length);
console.log("访问数组[0]:", arr[0]);
console.log("访问数组[4]:", arr[4]);

// 数组方法
var arr2 = [10, 20, 30];
var joined = arr2.join("-");
console.log("join方法结果:", joined);

var arr3 = [1, 2, 3];
arr3.push(4);
console.log("push(4)后的数组:", arr3);

var popped = arr3.pop();
console.log("pop返回值:", popped);
console.log("pop后的数组:", arr3);

// 数组遍历
console.log("遍历数组:");
var sum = 0;
for (var i = 0; i < arr.length; i++) {
    console.log("  arr[" + i + "] = " + arr[i]);
    sum += arr[i];
}
console.log("数组求和:", sum);

// ==================== 对象测试 ====================
console.log("\n===== 对象测试 =====");

// 对象创建和访问
var person = {
    name: "John Doe",
    age: 30,
    city: "New York",
    email: "john@example.com"
};

console.log("对象:", person.name, person.age, person.city);
console.log("访问对象属性.name:", person.name);
console.log("访问对象属性['age']:", person["age"]);

// 修改对象属性
person.age = 31;
console.log("修改age后:", person.age);

// 添加新属性
person.phone = "123-456-7890";
console.log("添加phone属性:", person.phone);

// 对象属性遍历
console.log("遍历对象属性:");
var keys = [];
for (var key in person) {
    console.log("  " + key + ": " + person[key]);
    keys.push(key);
}
console.log("对象的所有键:", keys.length, "个");

// ==================== 函数测试 ====================
console.log("\n===== 函数测试 =====");

// 基本函数定义和调用
function greet(name) {
    return "Hello, " + name + "!";
}
console.log("调用greet函数:", greet("Alice"));

// 多参数函数
function multiply(a, b) {
    return a * b;
}
console.log("3 * 7 =", multiply(3, 7));

// 递归函数 - 计算阶乘
function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}
console.log("5的阶乘:", factorial(5));

// 带有默认值的函数模拟
function sayHello(greeting, name) {
    if (greeting === undefined) greeting = "Hi";
    if (name === undefined) name = "Friend";
    return greeting + ", " + name + "!";
}
console.log(sayHello());
console.log(sayHello("Hello"));
console.log(sayHello("Hey", "Bob"));

// 函数作为参数
function applyOperation(a, b, operation) {
    return operation(a, b);
}

var result = applyOperation(5, 3, function(x, y) {
    return x + y;
});
console.log("applyOperation(5, 3, add):", result);

// 返回函数的函数
function makeAdder(x) {
    return function(y) {
        return x + y;
    };
}

var addFive = makeAdder(5);
console.log("makeAdder(5)(3):", addFive(3));

// ==================== 条件控制测试 ====================
console.log("\n===== 条件控制测试 =====");

// if-else 语句
var testNum = 10;
if (testNum > 0) {
    console.log(testNum, "是正数");
} else if (testNum < 0) {
    console.log(testNum, "是负数");
} else {
    console.log(testNum, "是零");
}

// 三元运算符
var age = 25;
var ageGroup = age >= 18 ? "成人" : "未成年";
console.log("年龄分组:", ageGroup);

// switch-case 语句
var day = 3;
var dayName = "";
switch (day) {
    case 1:
        dayName = "Monday";
        break;
    case 2:
        dayName = "Tuesday";
        break;
    case 3:
        dayName = "Wednesday";
        break;
    case 4:
        dayName = "Thursday";
        break;
    case 5:
        dayName = "Friday";
        break;
    default:
        dayName = "Weekend";
}
console.log("第" + day + "天是:", dayName);

// ==================== 循环测试 ====================
console.log("\n===== 循环测试 =====");

// for 循环
console.log("for循环 (1到5):");
for (var i = 1; i <= 5; i++) {
    console.log("  i =", i);
}

// while 循环
var counter = 0;
var whileResult = "";
while (counter < 3) {
    whileResult += counter + " ";
    counter++;
}
console.log("while循环结果:", whileResult);

// do-while 循环
var num = 0;
var doWhileCount = 0;
do {
    doWhileCount++;
    num++;
} while (num < 2);
console.log("do-while循环执行", doWhileCount, "次");

// 嵌套循环
console.log("嵌套循环 (3x3矩阵):");
for (var row = 1; row <= 3; row++) {
    var rowStr = "";
    for (var col = 1; col <= 3; col++) {
        rowStr += (row * col) + " ";
    }
    console.log("  " + rowStr);
}

// ==================== 字符串操作测试 ====================
console.log("\n===== 字符串操作测试 =====");

var str = "JavaScript";
console.log("原字符串:", str);
console.log("字符串长度:", str.length);
console.log("转大写:", str.toUpperCase());
console.log("转小写:", str.toLowerCase());

var str2 = "Hello World";
var index = str2.indexOf("World");
console.log("'World'在字符串中的位置:", index);

var substring = str2.substring(0, 5);
console.log("substring(0, 5):", substring);

var replaced = str2.replace("World", "JavaScript");
console.log("replace后:", replaced);

var splits = str2.split(" ");
console.log("split结果:", splits);

// ==================== 数学运算测试 ====================
console.log("\n===== 数学运算测试 =====");

var a = 10;
var b = 3;
console.log(a, "+", b, "=", a + b);
console.log(a, "-", b, "=", a - b);
console.log(a, "*", b, "=", a * b);
console.log(a, "/", b, "=", a / b);
console.log(a, "%", b, "=", a % b);
console.log(a, "**", b, "=", a ** b);

// 逻辑运算
console.log("true && false =", true && false);
console.log("true || false =", true || false);
console.log("!true =", !true);

// ==================== 对象方法测试 ====================
console.log("\n===== 对象方法测试 =====");

var car = {
    brand: "Toyota",
    model: "Camry",
    year: 2023,
    getInfo: function() {
        return this.year + " " + this.brand + " " + this.model;
    },
    honk: function() {
        return "Beep! Beep!";
    }
};

console.log("车辆信息:", car.getInfo());
console.log("鸣笛声:", car.honk());

// ==================== Class对象测试 ====================
console.log("\n===== Class对象测试 =====");

class Aclass {
  static bar = 42;
  static foo = { n: 100 };
}

class Bclass extends Aclass { // 通过 extends 继承
  constructor() {
    super();
    Bclass.foo.n--;
    Bclass.bar--;
  }
}

const bObj = new Bclass();
console.log('Bclass.foo.n = ', Bclass.foo.n) // 99
console.log('Bclass.bar = ', Bclass.bar) // 41
console.log('Aclass.foo.n = ', Aclass.foo.n) // 99
console.log('Aclass.bar = ', Aclass.bar) // 42

// ==================== 异常处理测试 ====================
console.log("\n===== 异常处理测试 =====");

try {
    var testVal = undefined.property; // 这会抛出错误
} catch (e) {
    console.log("捕获到异常:", "访问undefined的属性");
}

try {
    console.log("尝试执行...");
    // 正常执行
    var normalOp = 5 + 3;
    console.log("正常执行结果:", normalOp);
} catch (e) {
    console.log("异常");
} finally {
    console.log("finally块总是执行");
}

// ==================== 综合功能测试 ====================
console.log("\n===== 综合功能测试 =====");

// 定义一个复杂的类似对象的结构
function Student(name, grade, scores) {
    this.name = name;
    this.grade = grade;
    this.scores = scores;
}

// 为原型添加方法
Student.prototype.getAverage = function() {
    var sum = 0;
    for (var i = 0; i < this.scores.length; i++) {
        sum += this.scores[i];
    }
    return sum / this.scores.length;
};

Student.prototype.getGrade = function() {
    var avg = this.getAverage();
    if (avg >= 90) return "A";
    if (avg >= 80) return "B";
    if (avg >= 70) return "C";
    return "F";
};

var student1 = new Student("Alice", 10, [85, 90, 88, 92]);
var student2 = new Student("Bob", 10, [78, 82, 75, 80]);

console.log("学生1:", student1.name, "平均分:", student1.getAverage().toFixed(2), "等级:", student1.getGrade());
console.log("学生2:", student2.name, "平均分:", student2.getAverage().toFixed(2), "等级:", student2.getGrade());

// ==================== 数学函数测试 ====================
console.log("\n===== 数学函数测试 =====");
var sinVal = Math.sin(30 / 180 * Math.PI)
print(sinVal) // 0.5

var cosVal = Math.cos(60 / 180 * Math.PI)
print(cosVal) // 0.5

var tanVal = Math.tan(45 / 180 * Math.PI)
print(tanVal) // 1

var arcsinVal = Math.asin(1) / Math.PI * 180
print(arcsinVal) // 90

var arccosVal = Math.acos(0.5) / Math.PI * 180
print(arccosVal) // 60

var arctanVal = Math.atan(1) / Math.PI * 180
print(arctanVal) // 45

print(Math.random()) // 生成随机数
print(Math.floor(Math.PI)) // 向下取整
print(Math.ceil(Math.E)) // 向上取整
// ==================== 完成测试 ====================
console.log("\n===== 所有测试完成 =====");
console.log("测试已全部执行完毕!");
)";

#endif // MYSCRIPTCODE_H

