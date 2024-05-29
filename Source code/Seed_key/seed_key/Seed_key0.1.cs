namespace seed_key
{
    public partial class Seed_key : Form
    {
        public Seed_key()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {


        }

        private void button1_Click(object sender, EventArgs e)
        {
            if(string.IsNullOrWhiteSpace(textBox1.Text)|| string.IsNullOrWhiteSpace(textBox2.Text) || string.IsNullOrWhiteSpace(textBox3.Text) || string.IsNullOrWhiteSpace(textBox4.Text)) 
            {
                MessageBox.Show("Please enter the values", "Error",MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            int hex1Dec = Convert.ToInt32(textBox1.Text, 16);
            int hex2Dec = Convert.ToInt32(textBox2.Text, 16);
            int hex3Dec = Convert.ToInt32(textBox3.Text, 16);
            int hex4Dec = Convert.ToInt32(textBox4.Text, 16);
            int resultDec = hex4Dec + hex1Dec ^ (hex4Dec * hex2Dec) % hex3Dec;
            string resultHex = Convert.ToString(resultDec & 0xFFFF, 16).ToUpper().PadLeft(4, '0');
            textBox5.Text = resultHex;

        }
    }
}