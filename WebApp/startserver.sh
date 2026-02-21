cd ../OrangePI_SoC_Code
make clean
make
cd ../WebApp
gunicorn -b 0.0.0.0:5000 app:app
