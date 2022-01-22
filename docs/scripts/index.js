let time = 0;
let stars = [];

class Star {
	constructor() {
		this.x = Math.random() * cv.width;
		this.y = Math.random() * cv.height;
		this.brightness = Math.random();
		this.inc = Math.random() * (0.05 - 0.01) + 0.01;
		if (Math.random() < 0.5)
			this.inc *= -1;
	}

	display() {
		ctx.fillStyle = "rgba(255, 255, 255, " + this.brightness + ")";

		ctx.beginPath();
		ctx.arc(this.x, this.y, 1.5, 0, 2 * Math.PI);
		ctx.fill();

		this.brightness += this.inc;

		if (this.brightness >= 1) {
			this.inc *= -1;
		}
		else if (this.brightness <= 0) {
			this.inc *= -1;
			this.x = Math.random() * cv.width;
			this.y = Math.random() * cv.height;
		}
	}
}

let time = 0;

let stars = [];
for (let a = 0; a < 50; ++a)
	stars.push(new Star());



function draw() {
	//ctx.clearRect(0, 0, cv.width, cv.height);

	// background

	let clr = (Math.sin(time) * 50) + 10;
	time += 0.005;

	time += 0.005;

	// stars and cursor lines

	for (let a = 0; a < stars.length; ++a) {
		stars[a].display();
		
		if (a % 2 == 0 && dist(stars[a].x, stars[a].y, mouseX, mouseY) < 200) {
			stroke(255);
			strokeWeight(2);
			line(stars[a].x, stars[a].y, mouseX, mouseY);
		}
	}
}

let x, y;
function updateMouse(e) {
    x = e.clientX;
    y = e.clientY;
}

function dist(x1, y1, x2, y2) {
	return Math.sqrt(Math.pow(x1 - x2, 2) + Math.pow(y1 - y2, 2));
}



setInterval(draw, 20);